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

struct UInt96 {
  uint8 raw[96 / 8];
};

struct UInt128 {
  union {
    struct {
      int64 low;
      int64 high;
    };
    uint8 raw[128 / 8];
  };
};

struct UInt160 {
  uint8 raw[160 / 8];
};

struct UInt256 {
  union {
    struct {
      UInt128 low;
      UInt128 high;
    };
    uint8 raw[256 / 8];
  };
};

struct UInt2048 {
  uint8 raw[2048 / 8];
};

inline bool operator==(const UInt128 &a, const UInt128 &b) {
  return std::memcmp(a.raw, b.raw, sizeof(a)) == 0;
}

inline bool operator!=(const UInt128 &a, const UInt128 &b) {
  return std::memcmp(a.raw, b.raw, sizeof(a)) != 0;
}

inline bool operator==(const UInt160 &a, const UInt160 &b) {
  return std::memcmp(a.raw, b.raw, sizeof(a)) == 0;
}

inline bool operator!=(const UInt160 &a, const UInt160 &b) {
  return std::memcmp(a.raw, b.raw, sizeof(a)) != 0;
}

inline bool operator==(const UInt256 &a, const UInt256 &b) {
  return std::memcmp(a.raw, b.raw, sizeof(a)) == 0;
}

inline bool operator!=(const UInt256 &a, const UInt256 &b) {
  return std::memcmp(a.raw, b.raw, sizeof(a)) != 0;
}

}  // namespace td
