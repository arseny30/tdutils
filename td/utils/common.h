#pragma once

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
#include "td/utils/port/thread_local.h"

#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>  // for std::move and std::forward
#include <vector>

#include <cassert>  // TODO remove this header

#define ASSERT_CHECK assert

#define TD_DEBUG

#define TD_CONCAT_IMPL(x, y) x##y
#define TD_CONCAT(x, y) TD_CONCAT_IMPL(x, y)
#define TD_ANONYMOUS_VARIABLE(var) TD_CONCAT(TD_CONCAT(var, _), __LINE__)

// clang-format off
#if TD_WINDOWS
  #define TD_DIR_SLASH '\\'
#else
  #define TD_DIR_SLASH '/'
#endif
// clang-format on

namespace {
char no_empty_obj_ TD_UNUSED;  // to disable linker warning about empty files
}  // namespace

namespace td {

using string = std::string;
using wstring = std::wstring;

template <class ValueT>
using vector = std::vector<ValueT>;
template <class ValueT>
using unique_ptr = std::unique_ptr<ValueT>;
template <class ValueT>
using shared_ptr = std::shared_ptr<ValueT>;
template <class ValueT>
using weak_ptr = std::weak_ptr<ValueT>;

using std::make_shared;
using std::make_unique;

struct Unit {};

struct Auto {
  template <class ToT>
  operator ToT() const {
    return ToT();
  }
};

template <class ToT, class FromT>
ToT &as(FromT *from) {
  return *reinterpret_cast<ToT *>(from);
}

template <class ToT, class FromT>
const ToT &as(const FromT *from) {
  return *reinterpret_cast<const ToT *>(from);
}

template <class FunctionT>
class member_function_class {
 private:
  template <class ResultT, class ClassT, class... ArgsT>
  static auto helper(ResultT (ClassT::*x)(ArgsT...)) {
    return static_cast<ClassT *>(nullptr);
  }
  template <class ResultT, class ClassT, class... ArgsT>
  static auto helper(ResultT (ClassT::*x)(ArgsT...) const) {
    return static_cast<ClassT *>(nullptr);
  }

 public:
  using type = std::remove_pointer_t<decltype(helper(static_cast<FunctionT>(nullptr)))>;
};

template <class FunctionT>
using member_function_class_t = typename member_function_class<FunctionT>::type;

template <class T>
void reset(T &value) {
  using std::swap;
  std::decay_t<T> tmp;
  swap(tmp, value);
}

template <int Alignment, class T>
bool is_aligned_pointer(const T *pointer) {
  static_assert(Alignment > 0 && (Alignment & (Alignment - 1)) == 0, "Wrong alignment");
  return (reinterpret_cast<std::uintptr_t>(static_cast<const void *>(pointer)) & (Alignment - 1)) == 0;
}

}  // namespace td
