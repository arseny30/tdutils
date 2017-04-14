
#include <algorithm>
#include <cassert>
#include <iostream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "td/utils/logging.h"

namespace td {
namespace detail {

template <size_t... Args>
class MaxSizeImpl {};

template <size_t Res, size_t X, size_t... Args>
class MaxSizeImpl<Res, X, Args...> {
 public:
  static constexpr size_t value = MaxSizeImpl<std::max(Res, X), Args...>::value;
};

template <size_t Res>
class MaxSizeImpl<Res> {
 public:
  static constexpr size_t value = Res;
};

template <class... Args>
class MaxSize {
 public:
  static constexpr size_t value = MaxSizeImpl<0, sizeof(Args)...>::value;
};

template <size_t to_skip, class... Args>
class IthTypeImpl {};
template <class Res, class... Args>
class IthTypeImpl<0, Res, Args...> {
 public:
  using type = Res;
};
template <size_t pos, class Skip, class... Args>
class IthTypeImpl<pos, Skip, Args...> : public IthTypeImpl<pos - 1, Args...> {};

class Dummy {};

template <size_t pos, class... Args>
class IthType : public IthTypeImpl<pos, Args..., Dummy> {};

template <bool ok, int offset, class... Types>
class FindTypeOffsetImpl {};

template <int offset, class... Types>
class FindTypeOffsetImpl<true, offset, Types...> {
 public:
  static constexpr int value = offset;
};
template <int offset, class T, class S, class... Types>
class FindTypeOffsetImpl<false, offset, T, S, Types...>
    : public FindTypeOffsetImpl<std::is_same<T, S>::value, offset + 1, T, Types...> {};
template <class T, class... Types>
class FindTypeOffset : public FindTypeOffsetImpl<false, -1, T, Types...> {};

template <int offset, class... Types>
class ForEachTypeImpl {};

template <int offset>
class ForEachTypeImpl<offset, Dummy> {
 public:
  template <class F>
  static void visit(F &&f) {
  }
};

template <int offset, class T, class... Types>
class ForEachTypeImpl<offset, T, Types...> {
 public:
  template <class F>
  static void visit(F &&f) {
    f(offset, static_cast<T *>(nullptr));
    ForEachTypeImpl<offset + 1, Types...>::visit(f);
  }
};

template <class... Types>
class ForEachType {
 public:
  template <class F>
  static void visit(F &&f) {
    ForEachTypeImpl<0, Types..., Dummy>::visit(f);
  }
};

}  // namespace detail

template <class... Types>
class Variant {
 public:
  Variant() = default;
  Variant(const Variant &other);
  Variant(Variant &&other) {
    clear();
    other.visit([&](auto &&value) { this->init_empty(std::forward<decltype(value)>(value)); });
  }

  template <class T>
  Variant(T &&t) {
    init_empty(std::forward<T>(t));
  }
  template <class T>
  Variant &operator=(T &&t) {
    clear();
    init_empty(std::forward<T>(t));
    return *this;
  }

  template <class T>
  void init_empty(T &&t) {
    CHECK(offset_ == -1);
    offset_ = offset<T>();
    new (&get<T>()) T(std::forward<T>(t));
  }
  ~Variant() {
    clear();
  }

  template <class F>
  void visit(F &&f) {
    for_each([&](int offset, auto *ptr) {
      using T = std::decay_t<decltype(*ptr)>;
      if (offset == offset_) {
        f(std::move(this->get_unsafe<T>()));
      }
    });
  }

  template <class F>
  void for_each(F &&f) {
    detail::ForEachType<Types...>::visit(f);
  }

  template <class T>
  static constexpr int offset() {
    return detail::FindTypeOffset<T, Types...>::value;
  }
  template <int offset>
  auto &get() {
    CHECK(offset == offset_);
    return get_unsafe<offset>();
  }
  template <class T>
  auto &get() {
    return get<offset<T>()>();
  }

  void clear() {
    visit([](auto &&value) {
      using T = std::decay_t<decltype(value)>;
      value.~T();
    });
    offset_ = -1;
  }

  int offset_{-1};
  char data_[detail::MaxSize<Types...>::value];

 private:
  template <class T>
  auto &get_unsafe() {
    return *reinterpret_cast<T *>(data_);
  }

  template <int offset>
  auto &get_unsafe() {
    using T = typename detail::IthType<offset, Types...>::type;
    return get_unsafe<T>();
  }
};
}  // namespace td
