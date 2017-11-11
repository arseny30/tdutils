#include "td/utils/Random.h"

#if TD_HAVE_OPENSSL
#include <openssl/rand.h>
#endif

#include <algorithm>
#include <cstring>
#include <limits>
#include <random>

namespace td {

#if TD_HAVE_OPENSSL
void Random::secure_bytes(MutableSlice dest) {
  Random::secure_bytes(dest.ubegin(), dest.size());
}

void Random::secure_bytes(unsigned char *ptr, size_t size) {
  constexpr size_t buf_size = 512;
  static TD_THREAD_LOCAL unsigned char *buf;  // static zero-initialized
  static TD_THREAD_LOCAL size_t buf_pos;
  if (init_thread_local<unsigned char[]>(buf, buf_size)) {
    buf_pos = buf_size;
  }

  auto ready = std::min(size, buf_size - buf_pos);
  if (ready != 0) {
    std::memcpy(ptr, buf + buf_pos, ready);
    buf_pos += ready;
    ptr += ready;
    size -= ready;
    if (size == 0) {
      return;
    }
  }
  if (size < buf_size) {
    int err = RAND_bytes(buf, static_cast<int>(buf_size));
    // TODO: it CAN fail
    CHECK(err == 1);
    buf_pos = size;
    std::memcpy(ptr, buf, size);
    return;
  }

  CHECK(size <= static_cast<size_t>(std::numeric_limits<int>::max()));
  int err = RAND_bytes(ptr, static_cast<int>(size));
  // TODO: it CAN fail
  CHECK(err == 1);
}

int32 Random::secure_int32() {
  int32 res = 0;
  secure_bytes(reinterpret_cast<unsigned char *>(&res), sizeof(int32));
  return res;
}

int64 Random::secure_int64() {
  int64 res = 0;
  secure_bytes(reinterpret_cast<unsigned char *>(&res), sizeof(int64));
  return res;
}
#endif

static unsigned int rand_device_helper() {
  static TD_THREAD_LOCAL std::random_device *rd;
  init_thread_local<std::random_device>(rd);
  return (*rd)();
}

uint32 Random::fast_uint32() {
  static TD_THREAD_LOCAL std::mt19937 *gen;
  if (!gen) {
    auto &rg = rand_device_helper;
    std::seed_seq seq{rg(), rg(), rg(), rg(), rg(), rg(), rg(), rg(), rg(), rg(), rg(), rg()};
    init_thread_local<std::mt19937>(gen, seq);
  }
  return static_cast<uint32>((*gen)());
}

uint64 Random::fast_uint64() {
  static TD_THREAD_LOCAL std::mt19937_64 *gen;
  if (!gen) {
    auto &rg = rand_device_helper;
    std::seed_seq seq{rg(), rg(), rg(), rg(), rg(), rg(), rg(), rg(), rg(), rg(), rg(), rg()};
    init_thread_local<std::mt19937_64>(gen, seq);
  }
  return static_cast<uint64>((*gen)());
}

int Random::fast(int min, int max) {
  CHECK(0 <= min);
  CHECK(min <= max);
  return static_cast<int>(min + fast_uint32() % (max - min + 1));
}

}  // namespace td
