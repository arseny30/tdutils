#pragma once

#if TD_PORT_POSIX
#include <cerrno>
#include <type_traits>
#include "td/utils/Time.h"
#endif

namespace td {

#if TD_PORT_POSIX
namespace detail {
template <class F>
auto skip_eintr(F &&f) {
  decltype(f()) res;
  static_assert(std::is_integral<decltype(res)>::value, "integral type expected");
  do {
    errno = 0;  // just in case
    res = f();
  } while (res < 0 && errno == EINTR);
  return res;
}

template <class F>
auto skip_eintr_cstr(F &&f) {
  char *res;
  do {
    errno = 0;  // just in case
    res = f();
  } while (res == nullptr && errno == EINTR);
  return res;
}

template <class F>
auto skip_eintr_timeout(F &&f, int32 timeout_ms) {
  decltype(f(timeout_ms)) res;
  static_assert(std::is_integral<decltype(res)>::value, "integral type expected");

  auto start = Timestamp::now();
  auto left_timeout_ms = timeout_ms;
  while (true) {
    errno = 0;  // just in case
    res = f(left_timeout_ms);
    if (res >= 0 || errno != EINTR) {
      break;
    }
    left_timeout_ms = max(static_cast<int32>((start.at() - Timestamp::now().at()) * 1000 + timeout_ms + 1 - 1e-9), 0);
  }
  return res;
}
}  // namespace detail
#endif

}  // namespace td
