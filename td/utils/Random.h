#pragma once

#include "td/utils/common.h"
#include "td/utils/Slice.h"

namespace td {

class Random {
 public:
  static void secure_bytes(MutableSlice dest);
  static void secure_bytes(unsigned char *ptr, size_t size);
  static int32 secure_int32();
  static int64 secure_int64();
  static uint32 fast_uint32();
  static uint64 fast_uint64();

  // distribution is not uniform, min and max are included
  static int fast(int min, int max);
};

}  // namespace td
