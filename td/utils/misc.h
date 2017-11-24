#pragma once

#include "td/utils/common.h"
#include "td/utils/logging.h"
#include "td/utils/Slice.h"
#include "td/utils/Status.h"
#include "td/utils/StringBuilder.h"

#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <tuple>
#include <type_traits>
#include <utility>

namespace td {

inline char *str_dup(Slice str) {
  char *res = static_cast<char *>(std::malloc(str.size() + 1));
  if (res == nullptr) {
    return nullptr;
  }
  std::copy(str.begin(), str.end(), res);
  res[str.size()] = '\0';
  return res;
}

template <class T>
std::pair<T, T> split(T s, char delimiter = ' ') {
  auto delimiter_pos = s.find(delimiter);
  if (delimiter_pos == string::npos) {
    return {std::move(s), T()};
  } else {
    return {s.substr(0, delimiter_pos), s.substr(delimiter_pos + 1)};
  }
}

template <class T>
vector<T> full_split(T s, char delimiter = ' ') {
  T next;
  vector<T> result;
  while (!s.empty()) {
    std::tie(next, s) = split(s, delimiter);
    result.push_back(next);
  }
  return result;
}

inline string implode(vector<string> v, char delimiter = ' ') {
  string result;
  for (auto &str : v) {
    if (!result.empty()) {
      result += delimiter;
    }
    result += str;
  }
  return result;
}

template <class T, class Func>
auto transform(const T &v, const Func &f) {
  vector<decltype(f(*v.begin()))> result;
  result.reserve(v.size());
  for (auto &x : v) {
    result.push_back(f(x));
  }
  return result;
}

template <class T, class Func>
auto transform(T &&v, const Func &f) {
  vector<decltype(f(std::move(*v.begin())))> result;
  result.reserve(v.size());
  for (auto &x : v) {
    result.push_back(f(std::move(x)));
  }
  return result;
}

template <class T>
void reset_to_empty(T &value) {
  using std::swap;
  std::decay_t<T> tmp;
  swap(tmp, value);
}

template <class T>
auto append(vector<T> &destination, const vector<T> &source) {
  destination.insert(destination.end(), source.begin(), source.end());
}

template <class T>
auto append(vector<T> &destination, vector<T> &&source) {
  if (destination.empty()) {
    destination.swap(source);
    return;
  }
  destination.reserve(destination.size() + source.size());
  std::move(source.begin(), source.end(), std::back_inserter(destination));
  reset_to_empty(source);
}

inline bool begins_with(Slice str, Slice prefix) {
  return prefix.size() <= str.size() && prefix == Slice(str.data(), prefix.size());
}

inline bool ends_with(Slice str, Slice suffix) {
  return suffix.size() <= str.size() && suffix == Slice(str.data() + str.size() - suffix.size(), suffix.size());
}

inline char to_lower(char c) {
  if ('A' <= c && c <= 'Z') {
    return static_cast<char>(c - 'A' + 'a');
  }

  return c;
}

inline void to_lower_inplace(MutableSlice slice) {
  for (auto &c : slice) {
    c = to_lower(c);
  }
}

inline string to_lower(Slice slice) {
  auto result = slice.str();
  to_lower_inplace(result);
  return result;
}

inline char to_upper(char c) {
  if ('a' <= c && c <= 'z') {
    return static_cast<char>(c - 'a' + 'A');
  }

  return c;
}

inline void to_upper_inplace(MutableSlice slice) {
  for (auto &c : slice) {
    c = to_upper(c);
  }
}

inline string to_upper(Slice slice) {
  auto result = slice.str();
  to_upper_inplace(result);
  return result;
}

inline bool is_space(char c) {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\0' || c == '\v';
}

inline bool is_alpha(char c) {
  c |= 0x20;
  return 'a' <= c && c <= 'z';
}

inline bool is_digit(char c) {
  return '0' <= c && c <= '9';
}

inline bool is_alnum(char c) {
  return is_alpha(c) || is_digit(c);
}

inline bool is_hex_digit(char c) {
  if (is_digit(c)) {
    return true;
  }
  c |= 0x20;
  return 'a' <= c && c <= 'f';
}

template <class T>
T trim(T str) {
  auto begin = str.data();
  auto end = begin + str.size();
  while (begin < end && is_space(*begin)) {
    begin++;
  }
  while (begin < end && is_space(end[-1])) {
    end--;
  }
  if (static_cast<size_t>(end - begin) == str.size()) {
    return std::move(str);
  }
  return T(begin, end);
}

inline string oneline(Slice str) {
  string result;
  result.reserve(str.size());
  bool after_new_line = true;
  for (auto c : str) {
    if (c != '\n') {
      if (after_new_line) {
        if (c == ' ') {
          continue;
        }
        after_new_line = false;
      }
      result += c;
    } else {
      after_new_line = true;
      result += ' ';
    }
  }
  while (!result.empty() && result.back() == ' ') {
    result.pop_back();
  }
  return result;
}

template <class T>
std::enable_if_t<std::is_signed<T>::value, T> to_integer(Slice str) {
  using unsigned_T = typename std::make_unsigned<T>::type;
  unsigned_T integer_value = 0;
  auto begin = str.begin();
  auto end = str.end();
  bool is_negative = false;
  if (begin != end && *begin == '-') {
    is_negative = true;
    begin++;
  }
  while (begin != end && is_digit(*begin)) {
    integer_value = static_cast<unsigned_T>(integer_value * 10 + (*begin++ - '0'));
  }
  if (integer_value > static_cast<unsigned_T>(std::numeric_limits<T>::max())) {
    static_assert(~0 + 1 == 0, "Two's complement");
    // Use ~x + 1 instead of -x to suppress Visual Studio warning.
    integer_value = static_cast<unsigned_T>(~integer_value + 1);
    is_negative = !is_negative;

    if (integer_value > static_cast<unsigned_T>(std::numeric_limits<T>::max())) {
      return std::numeric_limits<T>::min();
    }
  }

  return is_negative ? static_cast<T>(-static_cast<T>(integer_value)) : static_cast<T>(integer_value);
}

template <class T>
std::enable_if_t<std::is_unsigned<T>::value, T> to_integer(Slice str) {
  T integer_value = 0;
  auto begin = str.begin();
  auto end = str.end();
  while (begin != end && is_digit(*begin)) {
    integer_value = static_cast<T>(integer_value * 10 + (*begin++ - '0'));
  }
  return integer_value;
}

template <class T>
Result<T> to_integer_safe(Slice str) {
  auto res = to_integer<T>(str);
  if (to_string(res) != str) {
    return Status::Error(PSLICE() << "Can't parse \"" << str << "\" as number");
  }
  return res;
}

inline int hex_to_int(char c) {
  if (is_digit(c)) {
    return c - '0';
  }
  c |= 0x20;
  if ('a' <= c && c <= 'f') {
    return c - 'a' + 10;
  }
  return 16;
}

template <class T>
typename std::enable_if<std::is_unsigned<T>::value, T>::type hex_to_integer(Slice str) {
  T integer_value = 0;
  auto begin = str.begin();
  auto end = str.end();
  while (begin != end && is_hex_digit(*begin)) {
    integer_value = static_cast<T>(integer_value * 16 + hex_to_int(*begin++));
  }
  return integer_value;
}

inline double to_double(CSlice str) {
  return std::atof(str.c_str());
}

template <class T>
T clamp(T value, T min_value, T max_value) {
  if (value < min_value) {
    return min_value;
  }
  if (value > max_value) {
    return max_value;
  }
  return value;
}

// run-time checked narrowing cast (type conversion):

namespace detail {
template <class T, class U>
struct is_same_signedness
    : public std::integral_constant<bool, std::is_signed<T>::value == std::is_signed<U>::value> {};

template <class T, class Enable = void>
struct safe_undeflying_type {
  using type = T;
};

template <class T>
struct safe_undeflying_type<T, std::enable_if_t<std::is_enum<T>::value>> {
  using type = std::underlying_type_t<T>;
};
}  // namespace detail

template <class R, class A>
R narrow_cast(const A &a) {
  using RT = typename detail::safe_undeflying_type<R>::type;
  using AT = typename detail::safe_undeflying_type<A>::type;

  static_assert(std::is_integral<RT>::value, "expected integral type to cast to");
  static_assert(std::is_integral<AT>::value, "expected integral type to cast from");

  auto r = R(a);
  CHECK(A(r) == a);
  CHECK((detail::is_same_signedness<RT, AT>::value) || ((static_cast<RT>(r) < RT{}) == (static_cast<AT>(a) < AT{})));

  return r;
}

template <class R, class A>
Result<R> narrow_cast_safe(const A &a) {
  using RT = typename detail::safe_undeflying_type<R>::type;
  using AT = typename detail::safe_undeflying_type<A>::type;

  static_assert(std::is_integral<RT>::value, "expected integral type to cast to");
  static_assert(std::is_integral<AT>::value, "expected integral type to cast from");

  auto r = R(a);
  if (!(A(r) == a)) {
    return Status::Error("Narrow cast failed");
  }
  if (!((detail::is_same_signedness<RT, AT>::value) || ((static_cast<RT>(r) < RT{}) == (static_cast<AT>(a) < AT{})))) {
    return Status::Error("Narrow cast failed");
  }

  return r;
}

template <int Alignment, class T>
bool is_aligned_pointer(const T *pointer) {
  static_assert(Alignment > 0 && (Alignment & (Alignment - 1)) == 0, "Wrong alignment");
  return (reinterpret_cast<std::uintptr_t>(static_cast<const void *>(pointer)) & (Alignment - 1)) == 0;
}

// overloaded
namespace detail {
template <class... Fs>
struct overload;

template <class F>
struct overload<F> : public F {
  explicit overload(F f) : F(f) {
  }
};
template <class F, class... Fs>
struct overload<F, Fs...>
    : public overload<F>
    , overload<Fs...> {
  overload(F f, Fs... fs) : overload<F>(f), overload<Fs...>(fs...) {
  }
  using overload<F>::operator();
  using overload<Fs...>::operator();
};
}  // namespace detail

template <class... F>
auto overloaded(F... f) {
  return detail::overload<F...>(f...);
}

}  // namespace td
