#include <gst/gst.h>

#include "auto-gst-object.hpp"
#include "macros/assert.hpp"
#include "macros/autoptr.hpp"

declare_autoptr(GstMessage, GstMessage, gst_message_unref);

auto add_new_element_to_pipeine(GstElement* const pipeline, const char* const element_name) -> GstElement* {
    auto elm = gst_element_factory_make(element_name, NULL);
    ensure(elm != NULL);
    ensure(gst_bin_add(GST_BIN(pipeline), elm) == TRUE);
    return elm;
}

auto run_pipeline(GstElement* pipeline) -> bool {
    ensure(gst_element_set_state(pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS);

    const auto bus = AutoGstObject(gst_element_get_bus(pipeline));
    ensure(bus.get() != NULL);
    const auto msg = AutoGstMessage(gst_bus_timed_pop_filtered(bus.get(),
                                                               GST_CLOCK_TIME_NONE,
                                                               GstMessageType(GST_MESSAGE_ERROR | GST_MESSAGE_EOS)));
    ensure(msg.get() != NULL);
    switch(GST_MESSAGE_TYPE(msg.get())) {
    case GST_MESSAGE_ERROR: {
        auto err = (GError*)(NULL);
        auto str = (gchar*)(NULL);
        gst_message_parse_error(msg.get(), &err, &str);
        g_printerr("Error received from element %s: %s\n", GST_OBJECT_NAME(msg->src), err->message);
        g_printerr("Debugging information: %s\n", str ? str : "none");
        g_clear_error(&err);
        g_free(str);
    } break;
    case GST_MESSAGE_EOS:
        g_print("End-Of-Stream reached.\n");
        break;
    default:
        g_printerr("Unexpected message received.\n");
        break;
    }

    ensure(gst_element_set_state(pipeline, GST_STATE_NULL) == GST_STATE_CHANGE_SUCCESS);
    return true;
}

