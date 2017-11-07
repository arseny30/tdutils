#pragma once

#include "td/utils/common.h"
#include "td/utils/Slice.h"

namespace td {

class Random {
 public:
#if TD_HAS_OPENSSL
  static void secure_bytes(MutableSlice dest);
  static void secure_bytes(unsigned char *ptr, size_t size);
  static int32 secure_int32();
  static int64 secure_int64();
#endif  // TD_HAS_OPENSSL
  static uint32 fast_uint32();
  static uint64 fast_uint64();

  // distribution is not uniform, min and max are included
  static int fast(int min, int max);
};

}  // namespace td
