#include "caps.hpp"
#include "macros/assert.hpp"
#include "macros/autoptr.hpp"

namespace {
declare_autoptr(GstCaps, GstCaps, gst_caps_unref);
}

auto set_caps(GstElement* const capsfilter, const char* const caps) -> bool {
    const auto c = AutoGstCaps(gst_caps_from_string(caps));
    ensure(c);
    g_object_set(capsfilter, "caps", c.get(), NULL);
    return true;
}
