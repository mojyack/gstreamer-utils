#include <gst/gst.h>

#include "gstutil/auto-gst-object.hpp"
#include "gstutil/pipeline-helper.hpp"
#include "macros/autoptr.hpp"
#include "macros/unwrap.hpp"

namespace {
declare_autoptr(GMainLoop, GMainLoop, g_main_loop_unref);
declare_autoptr(GstMessage, GstMessage, gst_message_unref);
declare_autoptr(GString, gchar, g_free);
} // namespace

auto main(int argc, char* argv[]) -> int {
    gst_init(&argc, &argv);

    const auto pipeline = AutoGstObject(gst_pipeline_new(NULL));
    ensure(pipeline.get() != NULL);

    // videotestsrc -> videoconvert -> waylandsink

    unwrap_mut(videotestsrc, add_new_element_to_pipeine(pipeline.get(), "videotestsrc"));
    unwrap_mut(videoconvert, add_new_element_to_pipeine(pipeline.get(), "videoconvert"));
    unwrap_mut(waylandsink, add_new_element_to_pipeine(pipeline.get(), "waylandsink"));

    g_object_set(&waylandsink,
                 "async", FALSE,
                 NULL);
    g_object_set(&videotestsrc,
                 "is-live", TRUE,
                 NULL);

    ensure(gst_element_link_pads(&videotestsrc, NULL, &videoconvert, NULL) == TRUE);
    ensure(gst_element_link_pads(&videoconvert, NULL, &waylandsink, NULL) == TRUE);

    ensure(run_pipeline(pipeline.get()));

    return 0;
}
