#pragma once

#include "td/utils/common.h"
#include "td/utils/logging.h"
#include "td/utils/misc.h"
#include "td/utils/Slice.h"
#include "td/utils/StackAllocator.h"
#include "td/utils/Status.h"
#include "td/utils/tl_parsers.h"
#include "td/utils/tl_storers.h"

#include <type_traits>
#include <unordered_set>

#define BEGIN_STORE_FLAGS() \
  do {                      \
    uint32 flags_store = 0; \
  uint32 bit_offset_store = 0

#define STORE_FLAG(flag)                     \
  flags_store |= (flag) << bit_offset_store; \
  bit_offset_store++

#define END_STORE_FLAGS()         \
  CHECK(bit_offset_store < 31);   \
  td::store(flags_store, storer); \
  }                               \
  while (false)

#define BEGIN_PARSE_FLAGS()      \
  do {                           \
    uint32 flags_parse;          \
    uint32 bit_offset_parse = 0; \
  td::parse(flags_parse, parser)

#define PARSE_FLAG(flag)                               \
  flag = ((flags_parse >> bit_offset_parse) & 1) != 0; \
  bit_offset_parse++

#define END_PARSE_FLAGS()                                                   \
  CHECK(bit_offset_parse < 31);                                             \
  CHECK((flags_parse & ~((1 << bit_offset_parse) - 1)) == 0)                \
      << flags_parse << " " << bit_offset_parse << " " << parser.version(); \
  }                                                                         \
  while (false)

#define END_PARSE_FLAGS_GENERIC()                                                                       \
  CHECK(bit_offset_parse < 31);                                                                         \
  CHECK((flags_parse & ~((1 << bit_offset_parse) - 1)) == 0) << flags_parse << " " << bit_offset_parse; \
  }                                                                                                     \
  while (false)

namespace td {
template <class StorerT>
void store(bool x, StorerT &storer) {
  storer.store_binary(static_cast<int32>(x));
}
template <class ParserT>
void parse(bool &x, ParserT &parser) {
  x = parser.fetch_int() != 0;
}

template <class StorerT>
void store(int32 x, StorerT &storer) {
  storer.store_binary(x);
}
template <class ParserT>
void parse(int32 &x, ParserT &parser) {
  x = parser.fetch_int();
}

template <class StorerT>
void store(uint32 x, StorerT &storer) {
  storer.store_binary(x);
}
template <class ParserT>
void parse(uint32 &x, ParserT &parser) {
  x = static_cast<uint32>(parser.fetch_int());
}

template <class StorerT>
void store(int64 x, StorerT &storer) {
  storer.store_binary(x);
}
template <class ParserT>
void parse(int64 &x, ParserT &parser) {
  x = parser.fetch_long();
}
template <class StorerT>
void store(uint64 x, StorerT &storer) {
  storer.store_binary(x);
}
template <class ParserT>
void parse(uint64 &x, ParserT &parser) {
  x = static_cast<uint64>(parser.fetch_long());
}

template <class StorerT>
void store(double x, StorerT &storer) {
  storer.store_binary(x);
}
template <class ParserT>
void parse(double &x, ParserT &parser) {
  x = parser.fetch_double();
}

template <class StorerT>
void store(Slice x, StorerT &storer) {
  storer.store_string(x);
}
template <class StorerT>
void store(const string &x, StorerT &storer) {
  storer.store_string(x);
}
template <class ParserT>
void parse(string &x, ParserT &parser) {
  x = parser.template fetch_string<string>();
}

template <class T, class StorerT>
void store(const vector<T> &vec, StorerT &storer) {
  storer.store_binary(narrow_cast<int32>(vec.size()));
  for (auto &val : vec) {
    store(val, storer);
  }
}
template <class T, class ParserT>
void parse(vector<T> &vec, ParserT &parser) {
  uint32 size = parser.fetch_int();
  if (parser.get_left_len() < size) {
    parser.set_error("Wrong vector length");
    return;
  }
  vec = vector<T>(size);
  for (auto &val : vec) {
    parse(val, parser);
  }
}

template <class Key, class Hash, class KeyEqual, class Allocator, class StorerT>
void store(const std::unordered_set<Key, Hash, KeyEqual, Allocator> &s, StorerT &storer) {
  storer.store_binary(narrow_cast<int32>(s.size()));
  for (auto &val : s) {
    store(val, storer);
  }
}
template <class Key, class Hash, class KeyEqual, class Allocator, class ParserT>
void parse(std::unordered_set<Key, Hash, KeyEqual, Allocator> &s, ParserT &parser) {
  uint32 size = parser.fetch_int();
  if (parser.get_left_len() < size) {
    parser.set_error("Wrong set length");
    return;
  }
  s.clear();
  for (uint32 i = 0; i < size; i++) {
    Key val;
    parse(val, parser);
    s.insert(std::move(val));
  }
}

template <class T, class StorerT>
std::enable_if_t<std::is_enum<T>::value> store(const T &val, StorerT &storer) {
  store(static_cast<int32>(val), storer);
}
template <class T, class ParserT>
std::enable_if_t<std::is_enum<T>::value> parse(T &val, ParserT &parser) {
  int32 result;
  parse(result, parser);
  val = static_cast<T>(result);
}

template <class T, class StorerT>
std::enable_if_t<!std::is_enum<T>::value> store(const T &val, StorerT &storer) {
  val.store(storer);
}
template <class T, class ParserT>
std::enable_if_t<!std::is_enum<T>::value> parse(T &val, ParserT &parser) {
  val.parse(parser);
}

template <class T>
string serialize(const T &object) {
  TlStorerCalcLength calc_length;
  store(object, calc_length);
  size_t length = calc_length.get_length();

  string key(length, '\0');
  if (!is_aligned_pointer<4>(key.data())) {
    auto ptr = StackAllocator::alloc(length);
    MutableSlice data = ptr.as_slice();
    TlStorerUnsafe storer(data.ubegin());
    store(object, storer);
    CHECK(storer.get_buf() == data.uend());
    key.assign(data.begin(), data.size());
  } else {
    MutableSlice data = key;
    TlStorerUnsafe storer(data.ubegin());
    store(object, storer);
    CHECK(storer.get_buf() == data.uend());
  }
  return key;
}

template <class T>
TD_WARN_UNUSED_RESULT Status unserialize(T &object, Slice data) {
  TlParser parser(data);
  parse(object, parser);
  parser.fetch_end();
  return parser.get_status();
}
}  // namespace td
