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
#include <functional>
#include <memory>
#include <string>
#include <tuple>  // for ctie implementation
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

class ObserverBase {
 public:
  virtual void notify() = 0;
  ObserverBase() = default;
  ObserverBase(const ObserverBase &) = delete;
  ObserverBase &operator=(const ObserverBase &) = delete;
  ObserverBase(ObserverBase &&) = delete;
  ObserverBase &operator=(ObserverBase &&) = delete;
  virtual ~ObserverBase() = default;
};

class Observer : ObserverBase {
 public:
  Observer() = default;
  explicit Observer(unique_ptr<ObserverBase> &&ptr) : observer_ptr_(std::move(ptr)) {
  }

  void notify() override {
    if (observer_ptr_) {
      observer_ptr_->notify();
    }
  }

 private:
  unique_ptr<ObserverBase> observer_ptr_;
};

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
  ~MovableValue() = default;

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

template <std::size_t... S>
struct IntSeq {};

template <std::size_t L, std::size_t N, std::size_t... S>
struct IntSeqGen : IntSeqGen<L, N - 1, L + N - 1, S...> {};

template <std::size_t L, std::size_t... S>
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

template <std::size_t N>
using IntRange = typename IntSeqGen<0, N>::type;

template <class F, class... Args, std::size_t... S>
void call_tuple_impl(F &func, std::tuple<Args...> &&tuple, IntSeq<S...>) {
  func(std::forward<Args>(std::get<S>(tuple))...);
}

template <class F, class... Args>
void call_tuple(F &func, std::tuple<Args...> &&tuple) {
  call_tuple_impl(func, std::move(tuple), IntRange<sizeof...(Args)>());
}

namespace detail {
template <class T>
struct is_reference_wrapper : std::false_type {};
template <class U>
struct is_reference_wrapper<std::reference_wrapper<U>> : std::true_type {};

template <class Base, class T, class Derived, class... Args>
auto INVOKE(T Base::*pmf, Derived &&ref,
            Args &&... args) noexcept(noexcept((std::forward<Derived>(ref).*pmf)(std::forward<Args>(args)...)))
    -> std::enable_if_t<std::is_function<T>::value && std::is_base_of<Base, std::decay<Derived>>::value,
                        decltype((std::forward<Derived>(ref).*pmf)(std::forward<Args>(args)...))> {
  return (std::forward<Derived>(ref).*pmf)(std::forward<Args>(args)...);
}

template <class Base, class T, class RefWrap, class... Args>
auto INVOKE(T Base::*pmf, RefWrap &&ref,
            Args &&... args) noexcept(noexcept((ref.get().*pmf)(std::forward<Args>(args)...)))
    -> std::enable_if_t<std::is_function<T>::value && is_reference_wrapper<std::decay_t<RefWrap>>::value,
                        decltype((ref.get().*pmf)(std::forward<Args>(args)...))>

{
  return (ref.get().*pmf)(std::forward<Args>(args)...);
}

template <class Base, class T, class Pointer, class... Args>
auto INVOKE(T Base::*pmf, Pointer &&ptr,
            Args &&... args) noexcept(noexcept(((*std::forward<Pointer>(ptr)).*pmf)(std::forward<Args>(args)...)))
    -> std::enable_if_t<std::is_function<T>::value && !is_reference_wrapper<std::decay_t<Pointer>>::value &&
                            !std::is_base_of<Base, std::decay_t<Pointer>>::value,
                        decltype(((*std::forward<Pointer>(ptr)).*pmf)(std::forward<Args>(args)...))> {
  return ((*std::forward<Pointer>(ptr)).*pmf)(std::forward<Args>(args)...);
}

template <class Base, class T, class Derived>
auto INVOKE(T Base::*pmd, Derived &&ref) noexcept(noexcept(std::forward<Derived>(ref).*pmd))
    -> std::enable_if_t<!std::is_function<T>::value && std::is_base_of<Base, std::decay_t<Derived>>::value,
                        decltype(std::forward<Derived>(ref).*pmd)> {
  return std::forward<Derived>(ref).*pmd;
}

template <class Base, class T, class RefWrap>
auto INVOKE(T Base::*pmd, RefWrap &&ref) noexcept(noexcept(ref.get().*pmd))
    -> std::enable_if_t<!std::is_function<T>::value && is_reference_wrapper<std::decay_t<RefWrap>>::value,
                        decltype(ref.get().*pmd)> {
  return ref.get().*pmd;
}

template <class Base, class T, class Pointer>
auto INVOKE(T Base::*pmd, Pointer &&ptr) noexcept(noexcept((*std::forward<Pointer>(ptr)).*pmd))
    -> std::enable_if_t<!std::is_function<T>::value && !is_reference_wrapper<std::decay_t<Pointer>>::value &&
                            !std::is_base_of<Base, std::decay_t<Pointer>>::value,
                        decltype((*std::forward<Pointer>(ptr)).*pmd)> {
  return (*std::forward<Pointer>(ptr)).*pmd;
}

template <class F, class... Args>
auto INVOKE(F &&f, Args &&... args) noexcept(noexcept(std::forward<F>(f)(std::forward<Args>(args)...)))
    -> std::enable_if_t<!std::is_member_pointer<std::decay_t<F>>::value,
                        decltype(std::forward<F>(f)(std::forward<Args>(args)...))> {
  return std::forward<F>(f)(std::forward<Args>(args)...);
}
}  // namespace detail

template <class F, class... ArgTypes>
auto invoke(F &&f, ArgTypes &&... args)
    // exception specification for QoI
    noexcept(noexcept(detail::INVOKE(std::forward<F>(f), std::forward<ArgTypes>(args)...)))
        -> decltype(detail::INVOKE(std::forward<F>(f), std::forward<ArgTypes>(args)...)) {
  return detail::INVOKE(std::forward<F>(f), std::forward<ArgTypes>(args)...);
}

template <class... Args, std::size_t... S>
void invoke_tuple_impl(std::tuple<Args...> &&tuple, IntSeq<S...>) {
  invoke(std::forward<Args>(std::get<S>(tuple))...);
}

template <class... Args>
void invoke_tuple(std::tuple<Args...> &&tuple) {
  invoke_tuple_impl(std::move(tuple), IntRange<sizeof...(Args)>());
}

template <class Actor, class F, class... Args, std::size_t... S>
void mem_call_tuple_impl(Actor *actor, F &func, std::tuple<Args...> &&tuple, IntSeq<S...>) {
  (actor->*func)(std::forward<Args>(std::get<S>(tuple))...);
}

template <class Actor, class F, class... Args>
void mem_call_tuple(Actor *actor, F &func, std::tuple<Args...> &&tuple) {
  mem_call_tuple_impl(actor, func, std::move(tuple), IntRange<sizeof...(Args)>());
}

template <class F, class... Args, std::size_t... S>
void tuple_for_each_impl(std::tuple<Args...> &tuple, const F &func, IntSeq<S...>) {
  const auto &dummy = {0, (func(std::get<S>(tuple)), 0)...};
  (void)dummy;
}

template <class F, class... Args>
void tuple_for_each(std::tuple<Args...> &tuple, const F &func) {
  tuple_for_each_impl(tuple, func, IntRange<sizeof...(Args)>());
}

template <class F, class... Args, std::size_t... S>
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
