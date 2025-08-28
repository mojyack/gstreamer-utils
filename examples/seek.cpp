#include <iostream>
#include <span>
#include <utility>

#include <gst/gst.h>

#include "gstutil/auto-gst-object.hpp"
#include "gstutil/pipeline-helper.hpp"
#include "macros/autoptr.hpp"
#include "macros/unwrap.hpp"
#include "util/charconv.hpp"
#include "util/split.hpp"

namespace {
declare_autoptr(GstQuery, GstQuery, gst_query_unref);
declare_autoptr(GString, gchar, g_free);

auto link_pads_simple(GstElement* const /*element*/, GstPad* const src_pad, GstElement* sink) -> void {
    const auto name = AutoGString(gst_pad_get_name(src_pad));
    PRINT("pad added {}", name.get());
    if(!std::string_view(name.get()).starts_with("video_")) {
        return;
    }

    auto sink_pad = AutoGstObject(gst_element_get_static_pad(sink, "sink"));
    ensure(sink_pad.get() != NULL);
    ensure(gst_pad_link(src_pad, sink_pad.get()) == GST_PAD_LINK_OK);
}

constexpr auto nano = size_t(1'000'000'000);

struct Player {
    GstElement* pipeline;
    double      rate = 1.0;

    auto set_state(GstState state) -> bool;

    auto play() -> bool;
    auto pause() -> bool;
    auto query_pos() -> std::optional<gint64>;
    auto seek_abs(GstClockTime pos) -> bool;
    auto seek_rel(int64_t diff) -> bool;
};

auto Player::set_state(const GstState state) -> bool {
    const auto ret = gst_element_set_state(pipeline, state);
    return ret == GST_STATE_CHANGE_SUCCESS || ret == GST_STATE_CHANGE_ASYNC;
}

auto Player::query_pos() -> std::optional<gint64> {
    auto query = AutoGstQuery(gst_query_new_position(GST_FORMAT_TIME));
    ensure(query);
    ensure(gst_element_query(pipeline, query.get()) == TRUE);

    auto pos = gint64();
    gst_query_parse_position(query.get(), NULL, &pos);
    return pos / nano;
}

auto Player::play() -> bool {
    return set_state(GST_STATE_PLAYING);
}

auto Player::pause() -> bool {
    return set_state(GST_STATE_PAUSED);
}

auto Player::seek_abs(GstClockTime pos) -> bool {
    pos *= nano; // sec to nanosec

    auto flags = GstSeekFlags(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT);
    auto event = (GstEvent*)(nullptr);
    if(rate > 0) {
        event = gst_event_new_seek(rate, GST_FORMAT_TIME, flags, GST_SEEK_TYPE_SET, pos, GST_SEEK_TYPE_END, 0);
    } else {
        event = gst_event_new_seek(rate, GST_FORMAT_TIME, flags, GST_SEEK_TYPE_SET, 0, GST_SEEK_TYPE_SET, pos);
    }
    ensure(gst_element_send_event(pipeline, event) == TRUE);
    return true;
}

auto Player::seek_rel(const int64_t diff) -> bool {
    unwrap(pos, query_pos());
    ensure(seek_abs(GstClockTime(pos) + diff));
    return true;
}

auto process_command(Player& player, const std::span<const std::string_view> args) -> bool {
    const auto command = args[0];
    if(command == "seek") {
        ensure(args.size() == 2);
        const auto time = args[1];
        const auto rel  = time[0] == '+' || time[0] == '-';
        unwrap(num, from_chars<int>(time[0] == '+' ? time.substr(1) : time));
        if(rel) {
            ensure(player.seek_rel(num));
        } else {
            ensure(num >= 0);
            ensure(player.seek_abs(num));
        }
    } else if(command == "play") {
        ensure(player.play());
    } else if(command == "pause") {
        ensure(player.pause());
    } else if(command == "pos") {
        unwrap(pos, player.query_pos());
        std::println("{}s", pos);
    } else {
        bail("unknown command");
    }
    return true;
}

auto cli(GstElement* pipeline) -> bool {
    auto player = Player{pipeline};
    auto line   = std::string();
loop:
    std::print("> ");
    std::flush(std::cout);
    if(!std::getline(std::cin, line)) {
        return true;
    }
    if(line.empty()) {
        goto loop;
    }
    const auto args = split_like_shell(line);
    if(args[0] == "exit") {
        return true;
    }
    if(process_command(player, args)) {
        std::println("done");
    }
    goto loop;
}
} // namespace

auto main(const int argc, const char* const* argv) -> int {
    auto video_file = "/tmp/video.mp4";
    if(argc == 2) {
        video_file = argv[1];
    }

    gst_init(NULL, NULL);

    const auto pipeline = AutoGstObject(gst_pipeline_new(NULL));
    ensure(pipeline);

    // filesrc -> qtdemux -> h264parse -> avdec_h264 -> videoconvert ! waylandsink
    unwrap_mut(filesrc, add_new_element_to_pipeline(pipeline.get(), "filesrc"));
    g_object_set(&filesrc, "location", video_file, NULL);
    unwrap_mut(qtdemux, add_new_element_to_pipeline(pipeline.get(), "qtdemux"));
    unwrap_mut(avdec_h264, add_new_element_to_pipeline(pipeline.get(), "avdec_h264"));
    unwrap_mut(videoconvert, add_new_element_to_pipeline(pipeline.get(), "videoconvert"));
    unwrap_mut(waylandsink, add_new_element_to_pipeline(pipeline.get(), "waylandsink"));

    ensure(gst_element_link_pads(&filesrc, NULL, &qtdemux, NULL) == TRUE);
    g_signal_connect(&qtdemux, "pad-added", G_CALLBACK(link_pads_simple), &avdec_h264);
    ensure(gst_element_link_pads(&avdec_h264, NULL, &videoconvert, NULL) == TRUE);
    ensure(gst_element_link_pads(&videoconvert, NULL, &waylandsink, NULL) == TRUE);

    std::println("state: {}", std::to_underlying(gst_element_set_state(pipeline.get(), GST_STATE_PLAYING)));

    cli(pipeline.get());

    ensure(gst_element_set_state(pipeline.get(), GST_STATE_NULL) == GST_STATE_CHANGE_SUCCESS);

    return 0;
}

