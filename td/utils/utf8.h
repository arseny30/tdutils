#pragma once

#include "Slice.h"

namespace td {

// checks UTF-8 string for correctness, replaces '\0' with ' ' if found
bool check_utf8(MutableCSlice str);

}  // end of namespace td
