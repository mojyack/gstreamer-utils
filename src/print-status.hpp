#pragma once
#include <gst/gst.h>

auto print_status_of_all(GstElement* const bin, int indent = 0) -> bool;
