#include "td/utils/utf8.h"

#include "td/utils/logging.h"  // for UNREACHABLE

namespace td {

bool check_utf8(CSlice str) {
  const char *data = str.data();
  const char *data_end = data + str.size();
  do {
    unsigned int a = static_cast<unsigned char>(*data++);
    if ((a & 0x80) == 0) {
      if (data == data_end + 1) {
        return true;
      }
      continue;
    }

#define ENSURE(condition) \
  if (!(condition)) {     \
    return false;         \
  }

    ENSURE((a & 0x40) != 0);

    unsigned int b = static_cast<unsigned char>(*data++);
    ENSURE((b & 0xc0) == 0x80);
    if ((a & 0x20) == 0) {
      ENSURE((a & 0x1e) > 0);
      continue;
    }

    unsigned int c = static_cast<unsigned char>(*data++);
    ENSURE((c & 0xc0) == 0x80);
    if ((a & 0x10) == 0) {
      int x = (((a & 0x0f) << 6) | (b & 0x20));
      ENSURE(x != 0 && x != 0x360);  // surrogates //-V560
      continue;
    }

    unsigned int d = static_cast<unsigned char>(*data++);
    ENSURE((d & 0xc0) == 0x80);
    if ((a & 0x08) == 0) {
      int t = (((a & 0x07) << 6) | (b & 0x30));
      ENSURE(0 < t && t < 0x110);  // end of unicode //-V560
      continue;
    }

    return false;
#undef ENSURE
  } while (1);

  UNREACHABLE();
}

const unsigned char *next_utf8_unsafe(const unsigned char *ptr, uint32 *code) {
  uint32 a = ptr[0];
  if ((a & 0x80) == 0) {
    if (code) {
      *code = a;
    }
    return ptr + 1;
  } else if ((a & 0x20) == 0) {
    if (code) {
      *code = ((a & 0x1f) << 6) | (ptr[1] & 0x3f);
    }
    return ptr + 2;
  } else if ((a & 0x10) == 0) {
    if (code) {
      *code = ((a & 0x0f) << 12) | ((ptr[1] & 0x3f) << 6) | (ptr[2] & 0x3f);
    }
    return ptr + 3;
  } else if ((a & 0x08) == 0) {
    if (code) {
      *code = ((a & 0x07) << 18) | ((ptr[1] & 0x3f) << 12) | ((ptr[2] & 0x3f) << 6) | (ptr[3] & 0x3f);
    }
    return ptr + 4;
  }
  UNREACHABLE();
}

}  // namespace td
