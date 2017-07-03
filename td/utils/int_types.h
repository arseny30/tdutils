#pragma once

#include "td/utils/port/platform.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace td {

#if !TD_WINDOWS
using size_t = std::size_t;
#endif

using int8 = std::int8_t;
using int16 = std::int16_t;
using uint16 = std::uint16_t;
using int32 = std::int32_t;
using uint32 = std::uint32_t;
using int64 = std::int64_t;
using uint64 = std::uint64_t;

static_assert(sizeof(std::uint8_t) == sizeof(unsigned char), "Unsigned char expected to be 8-bit");
using uint8 = unsigned char;

#if TD_MSVC
#pragma warning(push)
#pragma warning(disable : 4309)
#endif

static_assert(static_cast<char>(128) == -128 || static_cast<char>(128) == 128,
              "Unexpected cast to char implementation-defined behaviour");
static_assert(static_cast<char>(256) == 0, "Unexpected cast to char implementation-defined behaviour");
static_assert(static_cast<char>(-256) == 0, "Unexpected cast to char implementation-defined behaviour");

#if TD_MSVC
#pragma warning(pop)
#endif

template <size_t size>
struct UInt {
  static_assert(size % 8 == 0, "size should be divisible by 8");
  uint8 raw[size / 8];
};

template <size_t size>
inline bool operator==(const UInt<size> &a, const UInt<size> &b) {
  return std::memcmp(a.raw, b.raw, sizeof(a)) == 0;
}

template <size_t size>
inline bool operator!=(const UInt<size> &a, const UInt<size> &b) {
  return !(a == b);
}

using UInt128 = UInt<128>;
using UInt256 = UInt<256>;

}  // namespace td
