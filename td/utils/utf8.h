#pragma once

#include "Slice.h"

namespace td {

// checks UTF-8 string for correctness
bool check_utf8(CSlice str);
inline const uint8 *prev_utf8_unsafe(const uint8 *ptr) {
  while (((*--ptr) & 0xc0) == 0x80) {
    // pass
  }
  return ptr;
}

inline const uint8 *next_utf8_unsafe(const uint8 *ptr, uint32 *code) {
  uint32 a = ptr[0];
  if ((a & 0x80) == 0) {
    if (code) {
      *code = a;
    }
    return ptr + 1;
  } else if ((a & 0x20) == 0) {
    if (code) {
      *code = (a << 6) | (ptr[1] & 0x3f);
    }
    return ptr + 2;
  } else if ((a & 0x10) == 0) {
    if (code) {
      *code = (a << 12) | ((ptr[1] & 0x3f) << 6) | (ptr[2] & 0x3f);
    }
    return ptr + 3;
  } else if ((a & 0x08) == 0) {
    if (code) {
      *code = (a << 18) | ((ptr[1] & 0x3f) << 12) | ((ptr[2] & 0x3f) << 6) | (ptr[3] & 0x3f);
    }
    return ptr + 4;
  }
  UNREACHABLE();
}

}  // namespace td
