#include "print-status.hpp"
#include "macros/assert.hpp"
#include "macros/autoptr.hpp"

namespace {
declare_autoptr(GString, gchar, g_free);
}

auto print_status_of_all(GstElement* const bin, const int indent) -> bool {
    auto ie    = gst_bin_iterate_elements(GST_BIN(bin));
    auto value = GValue(G_VALUE_INIT);
    for(auto iter = gst_iterator_next(ie, &value); iter != GST_ITERATOR_DONE; iter = gst_iterator_next(ie, &value)) {
        if(iter != GST_ITERATOR_OK) {
            continue;
        }
        auto element = (GstElement*)g_value_peek_pointer(&value);
        auto current = GstState();
        auto pending = GstState();
        ensure(gst_element_get_state(element, &current, &pending, 100000) != GST_STATE_CHANGE_FAILURE);
        const auto element_name = AutoGString(gst_element_get_name(element));
        const auto type_name    = g_type_name(gst_element_factory_get_element_type(gst_element_get_factory(element)));
        PRINT("{}{}({}) state={} pending={}", std::string(indent, ' '), type_name, element_name.get(), gst_element_state_get_name(current), gst_element_state_get_name(pending));
        if(GST_IS_BIN(element)) {
            print_status_of_all(element, indent + 2);
        }
    }
    return true;
}
