#pragma once

/*** Platform macros ***/
#if defined(_WIN32)
// define something for Windows (32-bit and 64-bit, this part is common)
#if defined(_WIN64)
// define something for Windows (64-bit only)
#endif
#if defined(__cplusplus_winrt)
#define TD_WINRT 1
#endif
#if defined(__cplusplus_cli)
#define TD_CLI 1
#endif
#define TD_WINDOWS 1

#elif defined(__APPLE__)
#include "TargetConditionals.h"
#if TARGET_IPHONE_SIMULATOR
// iOS Simulator
#error "iOS Simulator is not supported"
#elif TARGET_OS_IPHONE
// iOS device
#error "iOS device is not supported"
#elif TARGET_OS_MAC
// Other kinds of Mac OS
#define TD_MAC 1
#else
// Unsupported platform
#error "Unknown Apple platform device is not supported"
#endif

#elif defined(__ANDROID__)
#define TD_ANDROID 1

#elif defined(__linux__)
// linux
#define TD_LINUX 1

#elif defined(__unix__)  // all unices not caught above
// Unix
#error "Unix is unsupported"

#else
#error "Unknown unsupported platform"
#endif

#if defined(__clang__)
// Clang
#define TD_CLANG 1
#elif defined(__ICC) || defined(__INTEL_COMPILER)
// Intel compiler
#define TD_INTEL 1
#elif defined(__GNUC__) || defined(__GNUG__)
// GNU GCC/G++.
#define TD_GCC 1
#elif defined(_MSC_VER)
// Microsoft Visual Studio
#define TD_MSVC 1
#else
#error "Unsupported compiler.."
#endif

#if TD_WINDOWS
#define NOMINMAX
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define _CRT_SECURE_NO_WARNINGS
#include <Winsock2.h>
#include <ws2tcpip.h>
#include <Mswsock.h>
#include <Windows.h>
#undef ERROR
#undef small
#endif

#if TD_MSVC
#pragma warning(disable : 4200)
#pragma warning(disable : 4996)
#pragma warning(disable : 4267)
#endif

#if TD_ANDROID
#define TD_THREAD_LOCAL __thread
#else
#define TD_HAS_CPP_THREAD_LOCAL 1
#define TD_THREAD_LOCAL thread_local
#endif

#if TD_MSVC
#define IF_NO_MSVC(...)
#define IF_MSVC(...) __VA_ARGS__
#else
#define IF_NO_MSVC(...) __VA_ARGS__
#define IF_MSVC(...)
#endif

#include "td/utils/int_types.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include <cassert>  // TODO remove this header

#define TD_DEBUG

#define ASSERT_CHECK assert

#if TD_GCC || TD_CLANG
#define WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#else
#define WARN_UNUSED_RESULT
#endif

#if TD_CLANG || TD_GCC
#define likely(x) __builtin_expect(x, 1)
#define unlikely(x) __builtin_expect(x, 0)
#else
#define likely(x) x
#define unlikely(x) x
#endif

#if TD_MSVC
#define TD_UNUSED __pragma(warning(suppress : 4100))
#elif TD_CLANG || TD_GCC
#define TD_UNUSED __attribute__((unused))
#else
#define TD_UNUSED
#endif

#define TD_CONCAT_IMPL(x, y) x##y
#define TD_CONCAT(x, y) TD_CONCAT_IMPL(x, y)
#define TD_ANONYMOUS_VARIABLE(var) TD_CONCAT(TD_CONCAT(var, _), __LINE__)

#if TD_WINDOWS
#define TD_DIR_SLASH '\\'
#else
#define TD_DIR_SLASH '/'
#endif

namespace {
char no_empty_obj_ TD_UNUSED;
}

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

class ObserverBase {
 public:
  virtual void notify() = 0;
  virtual ~ObserverBase() = default;
};

class Observer : ObserverBase {
 public:
  Observer() = default;
  Observer(unique_ptr<ObserverBase> &&ptr_) : observer_ptr_(std::move(ptr_)) {
  }
  Observer &operator=(Observer &&observer) {
    observer_ptr_ = std::move(observer.observer_ptr_);
    return *this;
  }
  Observer(const Observer &) = delete;
  Observer &operator=(const Observer &) = delete;

  void notify() override {
    if (observer_ptr_) {
      observer_ptr_->notify();
    }
  }

 private:
  unique_ptr<ObserverBase> observer_ptr_;
};

extern TD_THREAD_LOCAL int32 thread_id_;
inline void set_thread_id(int32 id) {
  thread_id_ = id;
}
inline int32 get_thread_id() {
  return thread_id_;
}

template <class T, T empty_val = T()>
class MovableValue {
 public:
  MovableValue() = default;
  MovableValue(T val) : val_(val) {
  }
  MovableValue(MovableValue &&other) : val_(other.val_) {
    other.clear();
  }
  MovableValue &operator=(MovableValue &&other) {
    val_ = other.val_;
    other.clear();
    return *this;
  }
  MovableValue(const MovableValue &) = delete;
  MovableValue &operator=(const MovableValue &) = delete;

  void clear() {
    val_ = empty_val;
  }
  const T &get() const {
    return val_;
  }

 private:
  T val_ = empty_val;
};

struct Unit {
  template <class... ArgsT>
  void operator()(ArgsT &&... args) {
  }
};

struct Auto {
  template <class ToT>
  operator ToT() const {
    return ToT();
  }
};

template <int... S>
struct IntSeq {};

template <int L, int N, int... S>
struct IntSeqGen : IntSeqGen<L, N - 1, L + N - 1, S...> {};

template <int L, int... S>
struct IntSeqGen<L, 0, S...> {
  typedef IntSeq<S...> type;
};

template <bool... Args>
class LogicAndImpl {};

template <bool Res, bool X, bool... Args>
class LogicAndImpl<Res, X, Args...> {
 public:
  static constexpr bool value = LogicAndImpl < Res && X, Args... > ::value;
};

template <bool Res>
class LogicAndImpl<Res> {
 public:
  static constexpr bool value = Res;
};

template <bool... Args>
class LogicAnd {
 public:
  static constexpr bool value = LogicAndImpl<true, Args...>::value;
};

template <int N>
using IntRange = typename IntSeqGen<0, N>::type;

template <class F, class... Args, int... S>
void call_tuple_impl(F &func, std::tuple<Args...> &&tuple, IntSeq<S...>) {
  func(std::forward<Args>(std::get<S>(tuple))...);
}

template <class F, class... Args>
void call_tuple(F &func, std::tuple<Args...> &&tuple) {
  call_tuple_impl(func, std::move(tuple), IntRange<sizeof...(Args)>());
}

template <class Actor, class F, class... Args, int... S>
void mem_call_tuple_impl(Actor *actor, F &func, std::tuple<Args...> &&tuple, IntSeq<S...>) {
  (actor->*func)(std::forward<Args>(std::get<S>(tuple))...);
}

template <class Actor, class F, class... Args>
void mem_call_tuple(Actor *actor, F &func, std::tuple<Args...> &&tuple) {
  mem_call_tuple_impl(actor, func, std::move(tuple), IntRange<sizeof...(Args)>());
}

template <class F, class... Args, int... S>
void tuple_for_each_impl(std::tuple<Args...> &tuple, const F &func, IntSeq<S...>) {
  const auto &dummy = {0, (func(std::get<S>(tuple)), 0)...};
  (void)dummy;
}

template <class F, class... Args>
void tuple_for_each(std::tuple<Args...> &tuple, const F &func) {
  tuple_for_each_impl(tuple, func, IntRange<sizeof...(Args)>());
}

template <class F, class... Args, int... S>
void tuple_for_each_impl(const std::tuple<Args...> &tuple, const F &func, IntSeq<S...>) {
  const auto &dummy = {0, (func(std::get<S>(tuple)), 0)...};
  (void)dummy;
}

template <class F, class... Args>
void tuple_for_each(const std::tuple<Args...> &tuple, const F &func) {
  tuple_for_each_impl(tuple, func, IntRange<sizeof...(Args)>());
}

template <class ToT, class FromT>
ToT &as(FromT *from) {
  return reinterpret_cast<ToT *>(from)[0];
}

template <class ToT, class FromT>
const ToT &as(const FromT *from) {
  return reinterpret_cast<const ToT *>(from)[0];
}

template <class... Args>
std::tuple<const Args &...> ctie(const Args &... args) WARN_UNUSED_RESULT;

template <class... Args>
std::tuple<const Args &...> ctie(const Args &... args) {
  return std::tie(args...);
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

}  // namespace td
