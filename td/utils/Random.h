#pragma once

#include "td/utils/common.h"
#include "td/utils/Slice.h"
#include "td/utils/Span.h"

namespace td {

class Random {
 public:
#if TD_HAVE_OPENSSL
  static void secure_bytes(MutableSlice dest);
  static void secure_bytes(unsigned char *ptr, size_t size);
  static int32 secure_int32();
  static int64 secure_int64();
  static uint32 secure_uint32();
  static uint64 secure_uint64();

  // works only for current thread
  static void add_seed(Slice bytes, double entropy = 0);
  static void secure_cleanup();
#endif

  static uint32 fast_uint32();
  static uint64 fast_uint64();

  // distribution is not uniform, min and max are included
  static int fast(int min, int max);
  static double fast(double min, double max);

  class Fast {
   public:
    uint64 operator()() {
      return fast_uint64();
    }
  };
  class Xorshift128plus {
   public:
    explicit Xorshift128plus(uint64 seed);
    Xorshift128plus(uint64 seed_a, uint64 seed_b);
    uint64 operator()();
    int fast(int min, int max);
    int64 fast64(int64 min, int64 max);
    void bytes(MutableSlice dest);

   private:
    uint64 seed_[2];
  };
};

template <class T, class R>
void random_shuffle(td::MutableSpan<T> v, R &rnd) {
  for (std::size_t i = 1; i < v.size(); i++) {
    auto pos = static_cast<std::size_t>(rnd() % (i + 1));
    std::swap(v[i], v[pos]);
  }
}

}  // namespace td
