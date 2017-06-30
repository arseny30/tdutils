#include "td/utils/Status.h"

namespace td {
CSlice strerror_safe(int code) {
  const size_t size = 1000;

  static TD_THREAD_LOCAL char *buf;  // static zero initialized
  init_thread_local<char[]>(buf, size);

#if TD_WINDOWS
  strerror_s(buf, size, code);
  return CSlice(buf, buf + std::strlen(buf));
#else
#if !defined(__GLIBC__) || ((_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600) && !_GNU_SOURCE)
  strerror_r(code, buf, size);
  return CSlice(buf, buf + std::strlen(buf));
#else
  return CSlice(strerror_r(code, buf, size));
#endif
#endif
}
}  // namespace td
