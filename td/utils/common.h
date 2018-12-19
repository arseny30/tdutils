#pragma once

#include "td/utils/config.h"
#include "td/utils/port/platform.h"

// clang-format off
#if TD_WINDOWS
  #ifndef NTDDI_VERSION
    #define NTDDI_VERSION 0x06020000
  #endif
  #ifndef WINVER
    #define WINVER 0x0602
  #endif
  #ifndef _WIN32_WINNT
    #define _WIN32_WINNT 0x0602
  #endif
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #ifndef UNICODE
    #define UNICODE
  #endif
  #ifndef _UNICODE
    #define _UNICODE
  #endif
  #ifndef _CRT_SECURE_NO_WARNINGS
    #define _CRT_SECURE_NO_WARNINGS
  #endif

  #include <Winsock2.h>
  #include <ws2tcpip.h>

  #include <Mswsock.h>
  #include <Windows.h>
  #undef ERROR
#endif
// clang-format on

#include "td/utils/int_types.h"
#include "td/utils/unique_ptr.h"

#include <string>
#include <vector>

#define TD_DEBUG

#define TD_DEFINE_STR_IMPL(x) #x
#define TD_DEFINE_STR(x) TD_DEFINE_STR_IMPL(x)
#define TD_CONCAT_IMPL(x, y) x##y
#define TD_CONCAT(x, y) TD_CONCAT_IMPL(x, y)

// clang-format off
#if TD_WINDOWS
  #define TD_DIR_SLASH '\\'
#else
  #define TD_DIR_SLASH '/'
#endif
// clang-format on

namespace td {

inline bool likely(bool x) {
#if TD_CLANG || TD_GCC || TD_INTEL
  return __builtin_expect(x, 1);
#else
  return x;
#endif
}

inline bool unlikely(bool x) {
#if TD_CLANG || TD_GCC || TD_INTEL
  return __builtin_expect(x, 0);
#else
  return x;
#endif
}

// replace std::max and std::min to not have to include <algorithm> everywhere
// as a side bonus, accept parameters by value, so constexpr variables aren't required to be instantiated
template <class T>
T max(T a, T b) {
  return a < b ? b : a;
}

template <class T>
T min(T a, T b) {
  return a < b ? a : b;
}

using string = std::string;

template <class ValueT>
using vector = std::vector<ValueT>;

struct Unit {};

struct Auto {
  template <class ToT>
  operator ToT() const {
    return ToT();
  }
};

template <class T>
class As {
 public:
  As(void *ptr) : ptr_(ptr) {
  }
  As(As &&) = default;
  const As<T> &operator=(const As &new_value) const {
    memcpy(ptr_, new_value.ptr_, sizeof(T));
    return *this;
  }
  const As<T> &operator=(const T new_value) const {
    memcpy(ptr_, &new_value, sizeof(T));
    return *this;
  }

  operator T() const {
    T res;
    memcpy(&res, ptr_, sizeof(T));
    return res;
  }

 private:
  void *ptr_;
};

template <class T>
class ConstAs {
 public:
  ConstAs(const void *ptr) : ptr_(ptr) {
  }

  operator T() const {
    T res;
    memcpy(&res, ptr_, sizeof(T));
    return res;
  }

 private:
  const void *ptr_;
};

template <class ToT, class FromT>
As<ToT> as(FromT *from) {
  return As<ToT>(from);
}

template <class ToT, class FromT>
const ConstAs<ToT> as(const FromT *from) {
  return ConstAs<ToT>(from);
}

}  // namespace td
