#include <iostream>
#include <thread>

#include <gst/gst.h>

#include "gstutil/auto-gst-object.hpp"
#include "gstutil/caps.hpp"
#include "gstutil/pipeline-helper.hpp"
#include "macros/autoptr.hpp"
#include "macros/unwrap.hpp"
#include "util/charconv.hpp"
#include "util/split.hpp"

namespace {
declare_autoptr(GMainLoop, GMainLoop, g_main_loop_unref);
declare_autoptr(GstMessage, GstMessage, gst_message_unref);
declare_autoptr(GstCaps, GstCaps, gst_caps_unref);
declare_autoptr(GString, gchar, g_free);

// callbacks
struct Context {
    GstElement* pipeline;
    GstElement* videotestsrc;
    GstElement* capsfilter1 = nullptr;
    GstElement* videorate   = nullptr;
    GstElement* videoscale  = nullptr;
    GstElement* capsfilter2 = nullptr;
    GstElement* waylandsink;
    int         width     = 1280;
    int         height    = 720;
    int         framerate = 30;
};

auto reconfigure_pipeline(Context& self) -> bool {
    // remove old elements
    for(const auto elem : std::array{self.capsfilter2, self.videoscale, self.videorate, self.capsfilter1}) {
        if(elem == nullptr) {
            continue;
        }
        ensure(gst_element_set_state(elem, GST_STATE_NULL) == GST_STATE_CHANGE_SUCCESS);
        ensure(gst_bin_remove(GST_BIN(self.pipeline), elem) == TRUE);
    }
    // create new elements
    unwrap_mut(capsfilter1, add_new_element_to_pipeline(self.pipeline, "capsfilter"));
    unwrap_mut(videorate, add_new_element_to_pipeline(self.pipeline, "videorate"));
    unwrap_mut(videoscale, add_new_element_to_pipeline(self.pipeline, "videoscale"));
    unwrap_mut(capsfilter2, add_new_element_to_pipeline(self.pipeline, "capsfilter"));
    self.capsfilter1 = &capsfilter1;
    self.videorate   = &videorate;
    self.videoscale  = &videoscale;
    self.capsfilter2 = &capsfilter2;

    // link
    const auto videorate_caps = AutoGstCaps(gst_caps_from_string(std::format("video/x-raw,framerate={}/1", self.framerate).data()));
    ensure(videorate_caps.get() != NULL);
    ensure(set_caps(&capsfilter1, "video/x-raw,format=RGBA,width=1280,height=720,framerate=30/1"));
    ensure(set_caps(&capsfilter2, std::format("video/x-raw,width={},height={}", self.width, self.height).data()));
    PRINT("link");
    ensure(gst_element_link_pads(self.videotestsrc, NULL, self.capsfilter1, NULL) == TRUE);
    ensure(gst_element_link_pads(self.capsfilter1, NULL, self.videorate, NULL) == TRUE);
    ensure(gst_element_link_pads_filtered(self.videorate, NULL, self.videoscale, NULL, videorate_caps.get()) == TRUE);
    ensure(gst_element_link_pads(self.videoscale, NULL, self.capsfilter2, NULL) == TRUE);
    ensure(gst_element_link_pads(self.capsfilter2, NULL, self.waylandsink, NULL) == TRUE);
    ensure(gst_element_sync_state_with_parent(self.capsfilter1) == TRUE);
    ensure(gst_element_sync_state_with_parent(self.videoscale) == TRUE);
    ensure(gst_element_sync_state_with_parent(self.videorate) == TRUE);
    ensure(gst_element_sync_state_with_parent(self.capsfilter2) == TRUE);
    return true;
}

auto pad_block_callback(GstPad* const /*pad*/, GstPadProbeInfo* const /*info*/, gpointer const data) -> GstPadProbeReturn {
    auto& self = *std::bit_cast<Context*>(data);
    PRINT("blocked");
    ASSERT(reconfigure_pipeline(self));
    PRINT("unblocking");
    return GST_PAD_PROBE_REMOVE;
}

auto run_change_resolution_example() -> bool {
    // videotestsrc -(RGB)> videorate -(30Hz)> videoscale -(720p)> waylandsink
    const auto pipeline = AutoGstObject(gst_pipeline_new(NULL));
    ensure(pipeline.get() != NULL);

    unwrap_mut(videotestsrc, add_new_element_to_pipeline(pipeline.get(), "videotestsrc"));
    g_object_set(&videotestsrc, "is-live", TRUE, "pattern", 11, "horizontal-speed", 2, NULL);
    unwrap_mut(waylandsink, add_new_element_to_pipeline(pipeline.get(), "waylandsink"));
    g_object_set(&waylandsink, "async", FALSE, NULL);

    auto context = Context{
        .pipeline     = pipeline.get(),
        .videotestsrc = &videotestsrc,
        .waylandsink  = &waylandsink,
    };

    ensure(reconfigure_pipeline(context));

    auto switcher = std::thread([&videotestsrc, &context]() -> bool {
        constexpr auto error_value = false;

        const auto src_pad = AutoGstObject(gst_element_get_static_pad(&videotestsrc, "src"));
        ensure_v(src_pad.get() != NULL);
        auto line = std::string();
        while(std::getline(std::cin, line)) {
#define error_act continue
            auto elms = split(line, " ");
            ensure_a(elms.size() == 3);
            unwrap_a(width, from_chars<int>(elms[0]));
            unwrap_a(height, from_chars<int>(elms[1]));
            unwrap_a(framerate, from_chars<int>(elms[2]));
            context.width     = width;
            context.height    = height;
            context.framerate = framerate;
            PRINT("changing to {}x{}@{}", width, height, framerate);
            gst_pad_add_probe(src_pad.get(), GST_PAD_PROBE_TYPE_BLOCK_DOWNSTREAM, pad_block_callback, &context, NULL);
#undef error_act
        }
        return true;
    });

    auto ret = run_pipeline(pipeline.get());
    switcher.join();
    return ret;
}
} // namespace

auto main(int argc, char* argv[]) -> int {
    gst_init(&argc, &argv);
    return run_change_resolution_example() ? 1 : 0;
}
