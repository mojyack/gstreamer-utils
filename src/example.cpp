#include <gst/gst.h>

#include "auto-gst-object.hpp"
#include "macros/autoptr.hpp"
#include "macros/unwrap.hpp"
#include "pipeline-helper.hpp"
#include "util/assert.hpp"

namespace {
declare_autoptr(GMainLoop, GMainLoop, g_main_loop_unref);
declare_autoptr(GstMessage, GstMessage, gst_message_unref);
declare_autoptr(GString, gchar, g_free);

auto run() -> bool {
    const auto pipeline = AutoGstObject(gst_pipeline_new(NULL));
    assert_b(pipeline.get() != NULL);

    // videotestsrc -> videoconvert -> waylandsink

    unwrap_pb_mut(videotestsrc, add_new_element_to_pipeine(pipeline.get(), "videotestsrc"));
    unwrap_pb_mut(videoconvert, add_new_element_to_pipeine(pipeline.get(), "videoconvert"));
    unwrap_pb_mut(waylandsink, add_new_element_to_pipeine(pipeline.get(), "waylandsink"));

    g_object_set(&waylandsink,
                 "async", FALSE,
                 NULL);
    g_object_set(&videotestsrc,
                 "is-live", TRUE,
                 NULL);

    assert_b(gst_element_link_pads(&videotestsrc, NULL, &videoconvert, NULL) == TRUE);
    assert_b(gst_element_link_pads(&videoconvert, NULL, &waylandsink, NULL) == TRUE);

    return run_pipeline(pipeline.get());
}
} // namespace

auto main(int argc, char* argv[]) -> int {
    gst_init(&argc, &argv);
    return run() ? 1 : 0;
}
