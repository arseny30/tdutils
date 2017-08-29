#pragma once

#include "td/utils/common.h"
#include "td/utils/logging.h"
#include "td/utils/Slice.h"

#include <algorithm>
#include <cstdlib>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <tuple>
#include <type_traits>
#include <utility>

namespace td {

struct ListNode {
  ListNode *next;
  ListNode *prev;
  ListNode() {
    clear();
  }

  ~ListNode() {
    remove();
  }

  ListNode(const ListNode &) = delete;
  ListNode &operator=(const ListNode &) = delete;

  ListNode(ListNode &&other) {
    if (other.empty()) {
      clear();
    } else {
      ListNode *head = other.prev;
      other.remove();
      head->put(this);
    }
  }

  ListNode &operator=(ListNode &&other) {
    this->remove();

    if (!other.empty()) {
      ListNode *head = other.prev;
      other.remove();
      head->put(this);
    }

    return *this;
  }

  void connect(ListNode *to) {
    CHECK(to != nullptr);
    next = to;
    to->prev = this;
  }

  void remove() {
    prev->connect(next);
    clear();
  }

  void put(ListNode *other) {
    other->connect(next);
    this->connect(other);
  }

  void put_back(ListNode *other) {
    prev->connect(other);
    other->connect(this);
  }

  ListNode *get() {
    ListNode *result = prev;
    if (result == this) {
      return nullptr;
    }
    result->prev->connect(this);
    result->clear();
    // this->connect(result->next);
    return result;
  }

  bool empty() const {
    return next == this;
  }

 private:
  void clear() {
    next = this;
    prev = this;
  }
};

class Named {
 public:
  Slice get_name() const {
    return name_;
  }
  void set_name(Slice name) {
    name_ = name.str();
  }

 private:
  string name_;
};

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
  vector<decltype(f(v[0]))> result;
  result.reserve(v.size());
  for (auto &x : v) {
    result.push_back(f(x));
  }
  return result;
}

template <class T, class Func>
auto transform(T &&v, const Func &f) {
  vector<decltype(f(std::move(v[0])))> result;
  result.reserve(v.size());
  for (auto &x : v) {
    result.push_back(f(std::move(x)));
  }
  return result;
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
  reset(source);
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

// run-time checked narrowing cast (type conversion):

namespace detail {
template <class T, class U>
struct is_same_signedness : public std::integral_constant<bool, std::is_signed<T>::value == std::is_signed<U>::value> {
};

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

template <class StatT>
class TimedStat {
 public:
  TimedStat(double duration, double now)
      : duration_(duration), current_(), current_timestamp_(now), next_(), next_timestamp_(now) {
  }
  TimedStat() : TimedStat(0, 0) {
  }
  template <class EventT>
  void add_event(const EventT &e, double now) {
    update(now);
    current_.on_event(e);
    next_.on_event(e);
  }
  const StatT &get_stat(double now) {
    update(now);
    return current_;
  }
  std::pair<StatT, double> stat_duration(double now) {
    update(now);
    return std::make_pair(current_, now - current_timestamp_);
  }
  void clear_events() {
    current_.clear();
    next_.clear();
  }

 private:
  double duration_;
  StatT current_;
  double current_timestamp_;
  StatT next_;
  double next_timestamp_;

  void update(double now) {
    CHECK(now >= next_timestamp_);
    if (duration_ == 0) {
      return;
    }
    if (next_timestamp_ + 2 * duration_ < now) {
      current_ = StatT();
      current_timestamp_ = now;
      next_ = StatT();
      next_timestamp_ = now;
    } else if (next_timestamp_ + duration_ < now) {
      current_ = next_;
      current_timestamp_ = next_timestamp_;
      next_ = StatT();
      next_timestamp_ = now;
    }
  }
};

class CounterStat {
 public:
  struct Event {};
  int32 count_ = 0;
  void on_event(Event e) {
    count_++;
  }
  void clear() {
    count_ = 0;
  }
};

class FloodControlFast {
 public:
  uint32 add_event(int32 now) {
    for (auto &limit : limits_) {
      limit.stat_.add_event(CounterStat::Event(), now);
      if (limit.stat_.get_stat(now).count_ > limit.count_) {
        wakeup_at_ = std::max(wakeup_at_, now + limit.duration_ * 2);
      }
    }
    return wakeup_at_;
  }
  uint32 get_wakeup_at() {
    return wakeup_at_;
  }

  void add_limit(uint32 duration, int32 count) {
    limits_.push_back({TimedStat<CounterStat>(duration, 0), duration, count});
  }

  void clear_events() {
    for (auto &limit : limits_) {
      limit.stat_.clear_events();
    }
    wakeup_at_ = 0;
  }

 private:
  uint32 wakeup_at_ = 0;
  struct Limit {
    TimedStat<CounterStat> stat_;
    uint32 duration_;
    int32 count_;
  };
  std::vector<Limit> limits_;
};

// More strict implementaions of flood control.
// Should be just fine for small counters.
class FloodControlStrict {
 public:
  int32 add_event(int32 now) {
    events_.push_back(Event{now});
    if (without_update_ > 0) {
      without_update_--;
    } else {
      update(now);
    }
    return wakeup_at_;
  }

  // no more than count in each duration.
  void add_limit(int32 duration, int32 count) {
    limits_.push_back(Limit{duration, count, 0});
  }

  int32 get_wakeup_at() {
    return wakeup_at_;
  }

  void clear_events() {
    events_.clear();
    for (auto &limit : limits_) {
      limit.pos_ = 0;
    }
    without_update_ = 0;
    wakeup_at_ = 0;
  }

  int32 update(int32 now) {
    size_t min_pos = events_.size();

    without_update_ = std::numeric_limits<size_t>::max();
    for (auto &limit : limits_) {
      if (limit.pos_ + limit.count_ < events_.size()) {
        limit.pos_ = events_.size() - limit.count_;
      }

      // binary-search? :D
      while (limit.pos_ < events_.size() && events_[limit.pos_].timestamp_ + limit.duration_ < now) {
        limit.pos_++;
      }

      if (limit.count_ + limit.pos_ <= events_.size()) {
        CHECK(limit.count_ + limit.pos_ == events_.size());
        wakeup_at_ = std::max(wakeup_at_, events_[limit.pos_].timestamp_ + limit.duration_);
        without_update_ = 0;
      } else {
        without_update_ = std::min(without_update_, limit.count_ + limit.pos_ - events_.size());
      }

      min_pos = std::min(min_pos, limit.pos_);
    }

    if (min_pos * 2 > events_.size()) {
      for (auto &limit : limits_) {
        limit.pos_ -= min_pos;
      }
      events_.erase(events_.begin(), events_.begin() + min_pos);
    }
    return wakeup_at_;
  }

 private:
  int32 wakeup_at_ = 0;
  struct Event {
    int32 timestamp_;
  };
  struct Limit {
    int32 duration_;
    int32 count_;
    size_t pos_;
  };
  size_t without_update_ = 0;
  std::vector<Event> events_;
  std::vector<Limit> limits_;
};

}  // namespace td
