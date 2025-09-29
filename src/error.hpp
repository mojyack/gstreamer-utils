#pragma once
#include <gst/gst.h>

#include "macros/autoptr.hpp"

declare_autoptr(GError, GError, g_error_free);
declare_autoptr(GString, gchar, g_free);

#pragma pop_macro("declare_autoptr")

inline auto parse_message_to_error(GstMessage* msg) -> std::pair<AutoGError, AutoGString> {
    auto err = AutoGError();
    auto str = AutoGString();
    gst_message_parse_error(msg, std::inout_ptr(err), std::inout_ptr(str));
    return {std::move(err), std::move(str)};
}
