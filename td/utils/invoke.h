#pragma once

#include "td/utils/common.h"

#include <functional>
#include <tuple>
#include <type_traits>

namespace td {

namespace detail {

template <std::size_t... S>
struct IntSeq {};

template <std::size_t L, std::size_t N, std::size_t... S>
struct IntSeqGen : IntSeqGen<L, N - 1, L + N - 1, S...> {};

template <std::size_t L, std::size_t... S>
struct IntSeqGen<L, 0, S...> {
  using type = IntSeq<S...>;
};

template <bool... Args>
class LogicAndImpl {};

template <bool Res, bool X, bool... Args>
class LogicAndImpl<Res, X, Args...> {
 public:
  static constexpr bool value = LogicAndImpl<(Res && X), Args...>::value;
};

template <bool Res>
class LogicAndImpl<Res> {
 public:
  static constexpr bool value = Res;
};

template <std::size_t N>
using IntRange = typename IntSeqGen<0, N>::type;

template <class T>
struct is_reference_wrapper : std::false_type {};

template <class U>
struct is_reference_wrapper<std::reference_wrapper<U>> : std::true_type {};

template <class Base, class T, class Derived, class... Args>
auto invoke_impl(T Base::*pmf, Derived &&ref,
                 Args &&... args) noexcept(noexcept((std::forward<Derived>(ref).*pmf)(std::forward<Args>(args)...)))
    -> std::enable_if_t<std::is_function<T>::value && std::is_base_of<Base, std::decay<Derived>>::value,
                        decltype((std::forward<Derived>(ref).*pmf)(std::forward<Args>(args)...))> {
  return (std::forward<Derived>(ref).*pmf)(std::forward<Args>(args)...);
}

template <class Base, class T, class RefWrap, class... Args>
auto invoke_impl(T Base::*pmf, RefWrap &&ref,
                 Args &&... args) noexcept(noexcept((ref.get().*pmf)(std::forward<Args>(args)...)))
    -> std::enable_if_t<std::is_function<T>::value && is_reference_wrapper<std::decay_t<RefWrap>>::value,
                        decltype((ref.get().*pmf)(std::forward<Args>(args)...))>

{
  return (ref.get().*pmf)(std::forward<Args>(args)...);
}

template <class Base, class T, class Pointer, class... Args>
auto invoke_impl(T Base::*pmf, Pointer &&ptr,
                 Args &&... args) noexcept(noexcept(((*std::forward<Pointer>(ptr)).*pmf)(std::forward<Args>(args)...)))
    -> std::enable_if_t<std::is_function<T>::value && !is_reference_wrapper<std::decay_t<Pointer>>::value &&
                            !std::is_base_of<Base, std::decay_t<Pointer>>::value,
                        decltype(((*std::forward<Pointer>(ptr)).*pmf)(std::forward<Args>(args)...))> {
  return ((*std::forward<Pointer>(ptr)).*pmf)(std::forward<Args>(args)...);
}

template <class Base, class T, class Derived>
auto invoke_impl(T Base::*pmd, Derived &&ref) noexcept(noexcept(std::forward<Derived>(ref).*pmd))
    -> std::enable_if_t<!std::is_function<T>::value && std::is_base_of<Base, std::decay_t<Derived>>::value,
                        decltype(std::forward<Derived>(ref).*pmd)> {
  return std::forward<Derived>(ref).*pmd;
}

template <class Base, class T, class RefWrap>
auto invoke_impl(T Base::*pmd, RefWrap &&ref) noexcept(noexcept(ref.get().*pmd))
    -> std::enable_if_t<!std::is_function<T>::value && is_reference_wrapper<std::decay_t<RefWrap>>::value,
                        decltype(ref.get().*pmd)> {
  return ref.get().*pmd;
}

template <class Base, class T, class Pointer>
auto invoke_impl(T Base::*pmd, Pointer &&ptr) noexcept(noexcept((*std::forward<Pointer>(ptr)).*pmd))
    -> std::enable_if_t<!std::is_function<T>::value && !is_reference_wrapper<std::decay_t<Pointer>>::value &&
                            !std::is_base_of<Base, std::decay_t<Pointer>>::value,
                        decltype((*std::forward<Pointer>(ptr)).*pmd)> {
  return (*std::forward<Pointer>(ptr)).*pmd;
}

template <class F, class... Args>
auto invoke_impl(F &&f, Args &&... args) noexcept(noexcept(std::forward<F>(f)(std::forward<Args>(args)...)))
    -> std::enable_if_t<!std::is_member_pointer<std::decay_t<F>>::value,
                        decltype(std::forward<F>(f)(std::forward<Args>(args)...))> {
  return std::forward<F>(f)(std::forward<Args>(args)...);
}

template <class F, class... ArgTypes>
auto invoke(F &&f,
            ArgTypes &&... args) noexcept(noexcept(invoke_impl(std::forward<F>(f), std::forward<ArgTypes>(args)...)))
    -> decltype(invoke_impl(std::forward<F>(f), std::forward<ArgTypes>(args)...)) {
  return invoke_impl(std::forward<F>(f), std::forward<ArgTypes>(args)...);
}

template <class F, class... Args, std::size_t... S>
void call_tuple_impl(F &func, std::tuple<Args...> &&tuple, IntSeq<S...>) {
  func(std::forward<Args>(std::get<S>(tuple))...);
}

template <class... Args, std::size_t... S>
void invoke_tuple_impl(std::tuple<Args...> &&tuple, IntSeq<S...>) {
  invoke(std::forward<Args>(std::get<S>(tuple))...);
}

template <class Actor, class F, class... Args, std::size_t... S>
void mem_call_tuple_impl(Actor *actor, F &func, std::tuple<Args...> &&tuple, IntSeq<S...>) {
  (actor->*func)(std::forward<Args>(std::get<S>(tuple))...);
}

template <class F, class... Args, std::size_t... S>
void tuple_for_each_impl(std::tuple<Args...> &tuple, const F &func, IntSeq<S...>) {
  const auto &dummy = {0, (func(std::get<S>(tuple)), 0)...};
  (void)dummy;
}

template <class F, class... Args, std::size_t... S>
void tuple_for_each_impl(const std::tuple<Args...> &tuple, const F &func, IntSeq<S...>) {
  const auto &dummy = {0, (func(std::get<S>(tuple)), 0)...};
  (void)dummy;
}

}  // namespace detail

template <bool... Args>
class LogicAnd {
 public:
  static constexpr bool value = detail::LogicAndImpl<true, Args...>::value;
};

template <class F, class... Args>
void call_tuple(F &func, std::tuple<Args...> &&tuple) {
  detail::call_tuple_impl(func, std::move(tuple), detail::IntRange<sizeof...(Args)>());
}

template <class... Args>
void invoke_tuple(std::tuple<Args...> &&tuple) {
  detail::invoke_tuple_impl(std::move(tuple), detail::IntRange<sizeof...(Args)>());
}

template <class Actor, class F, class... Args>
void mem_call_tuple(Actor *actor, F &func, std::tuple<Args...> &&tuple) {
  detail::mem_call_tuple_impl(actor, func, std::move(tuple), detail::IntRange<sizeof...(Args)>());
}

template <class F, class... Args>
void tuple_for_each(std::tuple<Args...> &tuple, const F &func) {
  detail::tuple_for_each_impl(tuple, func, detail::IntRange<sizeof...(Args)>());
}

template <class F, class... Args>
void tuple_for_each(const std::tuple<Args...> &tuple, const F &func) {
  detail::tuple_for_each_impl(tuple, func, detail::IntRange<sizeof...(Args)>());
}

}  // namespace td
