#pragma once
#include <gst/gst.h>

auto set_caps(GstElement* capsfilter, const char* caps) -> bool;
