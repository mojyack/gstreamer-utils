#include <chrono>
#include <thread>

#include <gst/gst.h>
#include <gst/video/gstvideodecoder.h>

#include "gstutil/auto-gst-object.hpp"
#include "gstutil/pipeline-helper.hpp"
#include "macros/autoptr.hpp"
#include "macros/unwrap.hpp"

namespace {
declare_autoptr(GMainLoop, GMainLoop, g_main_loop_unref);
declare_autoptr(GstMessage, GstMessage, gst_message_unref);
declare_autoptr(GString, gchar, g_free);

// callbacks
struct Context {
    GstElement* pipeline;
    GstElement* videotestsrc;
    // sink 1
    GstElement* fakesink;
    // sink 2
    GstElement* videoconvert;
    GstElement* waylandsink;
};

auto switch_fake_to_wayland(Context& self) -> bool {
    // remove old elements
    ensure(gst_element_set_state(self.fakesink, GST_STATE_NULL) == GST_STATE_CHANGE_SUCCESS);
    ensure(gst_bin_remove(GST_BIN(self.pipeline), self.fakesink) == TRUE);
    self.fakesink = nullptr;
    // create new elements
    unwrap_mut(videoconvert, add_new_element_to_pipeine(self.pipeline, "videoconvert"));
    unwrap_mut(waylandsink, add_new_element_to_pipeine(self.pipeline, "waylandsink"));
    self.videoconvert = &videoconvert;
    self.waylandsink  = &waylandsink;
    ensure(gst_element_link_pads(self.videotestsrc, NULL, &videoconvert, NULL) == TRUE);
    ensure(gst_element_link_pads(&videoconvert, NULL, &waylandsink, NULL) == TRUE);
    ensure(gst_element_sync_state_with_parent(&waylandsink) == TRUE);
    ensure(gst_element_sync_state_with_parent(&videoconvert) == TRUE);
    return true;
}

auto switch_wayland_to_fake(Context& self) -> bool {
    // remove old elements
    ensure(gst_element_set_state(self.videoconvert, GST_STATE_NULL) == GST_STATE_CHANGE_SUCCESS);
    ensure(gst_bin_remove(GST_BIN(self.pipeline), self.videoconvert) == TRUE);
    self.videoconvert = nullptr;
    ensure(gst_element_set_state(self.waylandsink, GST_STATE_NULL) == GST_STATE_CHANGE_SUCCESS);
    ensure(gst_bin_remove(GST_BIN(self.pipeline), self.waylandsink) == TRUE);
    self.waylandsink = nullptr;
    // create new elements
    unwrap_mut(fakesink, add_new_element_to_pipeine(self.pipeline, "fakesink"));
    self.fakesink = &fakesink;
    ensure(gst_element_link_pads(self.videotestsrc, NULL, &fakesink, NULL) == TRUE);
    ensure(gst_element_sync_state_with_parent(&fakesink) == TRUE);
    return true;
}
auto pad_block_callback(GstPad* const /*pad*/, GstPadProbeInfo* const /*info*/, gpointer const data) -> GstPadProbeReturn {
    auto& self = *std::bit_cast<Context*>(data);
    PRINT("blocked");
    if(self.fakesink != nullptr) {
        switch_fake_to_wayland(self);
    } else {
        switch_wayland_to_fake(self);
    }
    PRINT("unblocking");
    return GST_PAD_PROBE_REMOVE;
}

auto run_dynamic_switch_example() -> bool {
    const auto pipeline = AutoGstObject(gst_pipeline_new(NULL));
    ensure(pipeline.get() != NULL);

    unwrap_mut(videotestsrc, add_new_element_to_pipeine(pipeline.get(), "videotestsrc"));
    unwrap_mut(fakesink, add_new_element_to_pipeine(pipeline.get(), "fakesink"));

    g_object_set(&videotestsrc,
                 "is-live", TRUE,
                 NULL);

    g_object_set(&fakesink,
                 "async", FALSE,
                 NULL);

    auto context = Context{
        .pipeline     = pipeline.get(),
        .videotestsrc = &videotestsrc,
        .fakesink     = &fakesink,
        .videoconvert = nullptr,
        .waylandsink  = nullptr,
    };

    auto switcher = std::thread([&videotestsrc, &context]() -> bool {
        constexpr auto error_value = false;

        const auto src_pad = AutoGstObject(gst_element_get_static_pad(&videotestsrc, "src"));
        ensure_v(src_pad.get() != NULL);
        while(true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            PRINT("switch");
            gst_pad_add_probe(src_pad.get(), GST_PAD_PROBE_TYPE_BLOCK_DOWNSTREAM, pad_block_callback, &context, NULL);
        }
        return true;
    });

    ensure(gst_element_link_pads(&videotestsrc, NULL, &fakesink, NULL) == TRUE);

    auto ret = run_pipeline(pipeline.get());
    switcher.join();
    return ret;
}
} // namespace

auto main(int argc, char* argv[]) -> int {
    gst_init(&argc, &argv);
    return run_dynamic_switch_example() ? 1 : 0;
}
