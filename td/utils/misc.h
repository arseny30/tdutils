#pragma once

#include "td/utils/common.h"
#include "td/utils/Slice.h"
#include "td/utils/logging.h"
#include "td/utils/format.h"

#include <algorithm>
#include <initializer_list>
#include <iterator>
#include <random>
#include <tuple>
#include <type_traits>

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

struct UnreachableDeleter {
  template <class T>
  void operator()(T *) {
    UNREACHABLE();
  }
};

// 1. Allocate all objects in vector. (but vector never shrinks)
// 2. Id is safe way to reach this object.
// 3. All ids are unique.
// 4. All ids are non-zero.
//
//
template <class DataT>
class Container {
 public:
  using Id = uint64;
  DataT *get(Id id) {
    int slot_id = decode_id(id);
    if (slot_id == -1) {
      return nullptr;
    }
    return &slots_[slot_id].data;
  }

  void erase(Id id) {
    int slot_id = decode_id(id);
    if (slot_id == -1) {
      return;
    }
    release(slot_id);
  }

  DataT extract(Id id) {
    int slot_id = decode_id(id);
    CHECK(slot_id != -1);
    auto res = std::move(slots_[slot_id].data);
    release(slot_id);
    return res;
  }

  Id create(DataT &&data = DataT(), uint8 type = 0) {
    int id = store(std::move(data), type);
    return encode_id(id);
  }

  Id reset_id(Id id) {
    int slot_id = decode_id(id);
    CHECK(slot_id != -1);
    inc_generation(slot_id);
    return encode_id(slot_id);
  }

  static uint8 type_from_id(Id id) {
    return static_cast<uint8>(id);
  }

  std::vector<Id> ids() {
    std::vector<bool> is_bad(slots_.size(), false);
    for (auto id : empty_slots_) {
      is_bad[id] = true;
    }
    std::vector<Id> res;
    for (size_t i = 0, n = slots_.size(); i < n; i++) {
      if (!is_bad[i]) {
        res.push_back(encode_id(static_cast<int>(i)));
      }
    }
    return res;
  }
  template <class F>
  void for_each(const F &f) {
    auto ids = this->ids();
    for (auto id : ids) {
      f(id, *get(id));
    }
  }
  size_t size() const {
    CHECK(empty_slots_.size() <= slots_.size());
    return slots_.size() - empty_slots_.size();
  }
  bool empty() const {
    return size() == 0;
  }
  void clear() {
    *this = Container<DataT>();
  }

 private:
  static constexpr uint32 GENERATION_STEP = 1 << 8;
  static constexpr uint32 TYPE_MASK = (1 << 8) - 1;
  struct Slot {
    uint32 generation;
    DataT data;
  };
  vector<Slot> slots_;
  vector<int> empty_slots_;

  Id encode_id(int id) const {
    return (static_cast<uint64>(id) << 32) | slots_[id].generation;
  }

  int decode_id(Id id) const {
    int slot_id = static_cast<int>(id >> 32);
    uint32 generation = static_cast<uint32>(id);
    if (slot_id < 0 || slot_id >= static_cast<int>(slots_.size())) {
      return -1;
    }
    if (generation != slots_[slot_id].generation) {
      return -1;
    }
    return slot_id;
  }

  int store(DataT &&data, uint8 type) {
    int pos;
    if (!empty_slots_.empty()) {
      pos = empty_slots_.back();
      empty_slots_.pop_back();
      slots_[pos].data = std::move(data);
      slots_[pos].generation ^= (slots_[pos].generation & TYPE_MASK) ^ type;
    } else {
      CHECK(slots_.size() <= static_cast<size_t>(std::numeric_limits<int>::max()));
      pos = static_cast<int>(slots_.size());
      slots_.push_back(Slot{GENERATION_STEP + type, std::move(data)});
    }
    return pos;
  }

  void release(int id) {
    inc_generation(id);
    slots_[id].data = DataT();
    if (slots_[id].generation & ~TYPE_MASK) {  // generation overflow. Can't use this id anymore
      empty_slots_.push_back(id);
    }
  }

  void inc_generation(int id) {
    slots_[id].generation += GENERATION_STEP;
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

inline std::tuple<string, string> split(string s, char delimiter = ' ') {
  auto delimiter_pos = s.find(delimiter);
  if (delimiter_pos == string::npos) {
    return std::make_tuple(std::move(s), "");
  } else {
    auto head = s.substr(0, delimiter_pos);
    auto tail = s.substr(delimiter_pos + 1);
    return std::make_tuple(head, tail);
  }
}

inline vector<string> full_split(string s, char delimiter = ' ') {
  // TODO optimize
  string next;
  vector<string> result;
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
auto transform(const vector<T> &v, const Func &f) {
  vector<decltype(f(v[0]))> result;
  result.reserve(v.size());
  for (auto &x : v) {
    result.push_back(f(x));
  }
  return result;
}

template <class T, class Func>
auto transform(vector<T> &&v, const Func &f) {
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

inline char tolower(char c) {
  if ('A' <= c && c <= 'Z') {
    return static_cast<char>(c - 'A' + 'a');
  }

  return c;
}

inline void tolower_inplace(MutableSlice slice) {
  for (auto &c : slice) {
    c = tolower(c);
  }
}

inline string tolower(Slice slice) {
  auto result = slice.str();
  tolower_inplace(result);
  return result;
}

inline bool is_utf8_symbol_begin_char(unsigned char c) {
  return (c & 0xC0) != 0x80;
}

inline bool isspace(char c) {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\0' || c == '\v';
}

inline bool isalpha(char c) {
  c |= 0x20;
  return 'a' <= c && c <= 'z';
}

inline bool isdigit(char c) {
  return '0' <= c && c <= '9';
}

inline bool ishexdigit(char c) {
  if (isdigit(c)) {
    return true;
  }
  c |= 0x20;
  return 'a' <= c && c <= 'f';
}

template <class T>
T trim(T str) {
  auto begin = str.data();
  auto end = begin + str.size();
  while (begin < end && isspace(*begin)) {
    begin++;
  }
  while (begin < end && isspace(end[-1])) {
    end--;
  }
  if (static_cast<size_t>(end - begin) == str.size()) {
    return std::move(str);
  }
  return T(begin, end);
}

template <class T>
T utf8_truncate(T str, size_t length) {
  for (size_t i = 0; i < str.size(); i++) {
    if (is_utf8_symbol_begin_char(static_cast<unsigned char>(str[i]))) {
      if (length == 0) {
        return str.substr(0, i);
      } else {
        length--;
      }
    }
  }
  return std::move(str);
}

inline string oneline(Slice str) {
  string result;
  result.reserve(str.size());
  for (auto c : str) {
    if (c != '\n') {
      result += c;
    }
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
  while (begin != end && isdigit(*begin)) {
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
  while (begin != end && isdigit(*begin)) {
    integer_value = static_cast<T>(integer_value * 10 + (*begin++ - '0'));
  }
  return integer_value;
}

inline int hex_to_int(char c) {
  if (isdigit(c)) {
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
  while (begin != end && ishexdigit(*begin)) {
    integer_value = static_cast<T>(integer_value * 16 + hex_to_int(*begin++));
  }
  return integer_value;
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
}

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
    if (duration_ == 0) {  //-V550
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

inline auto rand_device_helper() {
  static std::random_device rd;
  return rd();
}

inline uint32 rand_fast_uint32() {
  static std::mt19937 gen(rand_device_helper());
  return static_cast<uint32>(gen());
}
inline uint64 rand_fast_uint64() {
  static std::mt19937_64 gen(rand_device_helper());
  return static_cast<uint64>(gen());
}

// Process changes after they are finished in order of addition
template <class DataT>
class ChangesProcessor {
 public:
  using Id = uint64;

  void clear() {
    offset_ += data_array_.size();
    ready_i_ = 0;
    data_array_.clear();
  }

  template <class FromDataT>
  Id add(FromDataT &&data) {
    auto res = offset_ + data_array_.size();
    data_array_.emplace_back(std::forward<DataT>(data), false);
    return static_cast<Id>(res);
  }

  template <class F>
  void finish(Id token, F &&func) {
    size_t pos = static_cast<size_t>(token) - offset_;
    if (pos >= data_array_.size()) {
      return;
    }
    data_array_[pos].second = true;
    while (ready_i_ < data_array_.size() && data_array_[ready_i_].second == true) {
      func(std::move(data_array_[ready_i_].first));
      ready_i_++;
    }
    try_compactify();
  }

 private:
  size_t offset_ = 1;
  size_t ready_i_ = 0;
  std::vector<std::pair<DataT, bool>> data_array_;
  void try_compactify() {
    if (ready_i_ > 5 && ready_i_ * 2 > data_array_.size()) {
      data_array_.erase(data_array_.begin(), data_array_.begin() + ready_i_);
      offset_ += ready_i_;
      ready_i_ = 0;
    }
  }
};

// Process states in order defined by their Id
template <class DataT>
class OrderedEventsProcessor {
 public:
  using SeqNo = uint64;

  OrderedEventsProcessor() = default;
  explicit OrderedEventsProcessor(SeqNo offset) : offset_(offset), begin_(offset_), end_(offset_) {
  }

  template <class FromDataT, class FunctionT>
  void add(SeqNo seq_no, FromDataT &&data, FunctionT &&function) {
    CHECK(seq_no >= begin_) << seq_no << ">=" << begin_;  // or ignore?

    if (seq_no == begin_) {  // run now
      begin_++;
      function(seq_no, std::move(data));

      while (begin_ < end_) {
        auto &data_flag = data_array_[static_cast<size_t>(begin_ - offset_)];
        if (!data_flag.second) {
          break;
        }
        function(begin_, std::move(data_flag.first));
        data_flag.second = false;
        begin_++;
      }
      if (begin_ > end_) {
        end_ = begin_;
      }
      if (begin_ == end_) {
        offset_ = begin_;
      }

      // try_compactify
      auto begin_pos = static_cast<size_t>(begin_ - offset_);
      if (begin_pos > 5 && begin_pos * 2 > data_array_.size()) {
        data_array_.erase(data_array_.begin(), data_array_.begin() + begin_pos);
        offset_ = begin_;
      }
    } else {
      auto pos = static_cast<size_t>(seq_no - offset_);
      CHECK(pos <= 10000) << tag("pos", pos) << tag("seq_no", seq_no) << tag("offset_seq_no", offset_);
      auto need_size = pos + 1;
      if (data_array_.size() < need_size) {
        data_array_.resize(need_size);
      }
      data_array_[pos].first = std::move(data);
      data_array_[pos].second = true;
      if (end_ < seq_no + 1) {
        end_ = seq_no + 1;
      }
    }
  }

  bool has_events() const {
    return begin_ != end_;
  }
  SeqNo max_unfinished_seq_no() {
    return end_ - 1;
  }
  SeqNo max_finished_seq_no() {
    return begin_ - 1;
  }

 private:
  SeqNo offset_ = 1;
  SeqNo begin_ = 1;
  SeqNo end_ = 1;
  std::vector<std::pair<DataT, bool>> data_array_;
};

class PathView {
 public:
  explicit PathView(Slice path) : path_(path) {
    last_slash_ = static_cast<int32>(path_.size()) - 1;
    while (last_slash_ >= 0 && !is_slash(path_[last_slash_])) {
      last_slash_--;
    }

    last_dot_ = static_cast<int32>(path_.size());
    for (auto i = last_dot_ - 1; i > last_slash_ + 1; i--) {
      if (path_[i] == '.') {
        last_dot_ = i;
        break;
      }
    }
  }

  bool empty() const {
    return path_.empty();
  }
  bool is_dir() const {
    if (empty()) {
      return false;
    }
    return is_slash(path_.back());
  }
  Slice parent_dir() const {
    return Slice(path_.begin(), last_slash_ + 1);
  }
  Slice extension() const {
    if (last_dot_ == static_cast<int32>(path_.size())) {
      return Slice();
    }
    return path_.substr(last_dot_ + 1);
  }
  Slice without_extension() const {
    return Slice(path_.begin(), last_dot_);
  }
  Slice file_stem() const {
    return path_.substr(last_slash_ + 1, last_dot_ - last_slash_ - 1);
  }
  Slice file_name() const {
    return path_.substr(last_slash_ + 1);
  }
  Slice path() const {
    return path_;
  }

  bool is_absolute() const {
    return !empty() && is_slash(path_[0]);
  }
  bool is_relative() const {
    return !is_absolute();
  }

  static Slice relative(Slice path, Slice dir, bool force = false) {
    if (begins_with(path, dir)) {
      path.remove_prefix(dir.size());
      return path;
    }
    if (force) {
      return Slice();
    }
    return path;
  }
  static Slice dir_and_file(Slice path) {
    auto last_slash = static_cast<int32>(path.size()) - 1;
    while (last_slash >= 0 && !is_slash(path[last_slash])) {
      last_slash--;
    }
    if (last_slash < 0) {
      return Slice();
    }
    last_slash--;
    while (last_slash >= 0 && !is_slash(path[last_slash])) {
      last_slash--;
    }
    if (last_slash < 0) {
      return Slice();
    }
    return path.substr(last_slash + 1);
  }

 private:
  static bool is_slash(char c) {
    return c == '/' || c == '\\';
  }

  Slice path_;
  int32 last_slash_;
  int32 last_dot_;
};

}  // namespace td
