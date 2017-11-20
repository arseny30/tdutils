#pragma once

#include "td/utils/buffer.h"  // for BufferSlice
#include "td/utils/common.h"
#include "td/utils/misc.h"
#include "td/utils/Slice.h"
#include "td/utils/StackAllocator.h"
#include "td/utils/Status.h"
#include "td/utils/tl_parsers.h"
#include "td/utils/tl_storers.h"

#include <type_traits>
#include <unordered_set>

#define BEGIN_STORE_FLAGS() \
  uint32 flags_store = 0;   \
  uint32 bit_offset_store = 0

#define STORE_FLAG(flag)                     \
  flags_store |= (flag) << bit_offset_store; \
  bit_offset_store++

#define END_STORE_FLAGS()       \
  CHECK(bit_offset_store < 32); \
  td::store(flags_store, storer)

#define BEGIN_PARSE_FLAGS()    \
  uint32 flags_parse;          \
  uint32 bit_offset_parse = 0; \
  td::parse(flags_parse, parser)

#define PARSE_FLAG(flag)                               \
  flag = ((flags_parse >> bit_offset_parse) & 1) != 0; \
  bit_offset_parse++

#define END_PARSE_FLAGS() CHECK(bit_offset_parse < 32)

namespace td {
template <class Storer>
void store(bool x, Storer &storer) {
  storer.store_binary(static_cast<int32>(x));
}
template <class Parser>
void parse(bool &x, Parser &parser) {
  x = parser.fetch_int() != 0;
}

template <class Storer>
void store(int32 x, Storer &storer) {
  storer.store_binary(x);
}
template <class Parser>
void parse(int32 &x, Parser &parser) {
  x = parser.fetch_int();
}

template <class Storer>
void store(uint32 x, Storer &storer) {
  storer.store_binary(x);
}
template <class Parser>
void parse(uint32 &x, Parser &parser) {
  x = static_cast<uint32>(parser.fetch_int());
}

template <class Storer>
void store(int64 x, Storer &storer) {
  storer.store_binary(x);
}
template <class Parser>
void parse(int64 &x, Parser &parser) {
  x = parser.fetch_long();
}
template <class Storer>
void store(uint64 x, Storer &storer) {
  storer.store_binary(x);
}
template <class Parser>
void parse(uint64 &x, Parser &parser) {
  x = static_cast<uint64>(parser.fetch_long());
}

template <class Storer>
void store(double x, Storer &storer) {
  storer.store_binary(x);
}
template <class Parser>
void parse(double &x, Parser &parser) {
  x = parser.fetch_double();
}

template <class Storer>
void store(Slice x, Storer &storer) {
  storer.store_string(x);
}
template <class Storer>
void store(const string &x, Storer &storer) {
  storer.store_string(x);
}
template <class Parser>
void parse(string &x, Parser &parser) {
  x = parser.template fetch_string<string>();
}

template <class Storer>
void store(const BufferSlice &x, Storer &storer) {
  storer.store_string(x);
}
template <class Parser>
void parse(BufferSlice &x, Parser &parser) {
  x = parser.template fetch_string<BufferSlice>();
}

template <class T, class Storer>
void store(const vector<T> &vec, Storer &storer) {
  storer.store_binary(narrow_cast<int32>(vec.size()));
  for (auto &val : vec) {
    store(val, storer);
  }
}
template <class T, class Parser>
void parse(vector<T> &vec, Parser &parser) {
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

template <class Key, class Hash, class KeyEqual, class Allocator, class Storer>
void store(const std::unordered_set<Key, Hash, KeyEqual, Allocator> &s, Storer &storer) {
  storer.store_binary(narrow_cast<int32>(s.size()));
  for (auto &val : s) {
    store(val, storer);
  }
}
template <class Key, class Hash, class KeyEqual, class Allocator, class Parser>
void parse(std::unordered_set<Key, Hash, KeyEqual, Allocator> &s, Parser &parser) {
  uint32 size = parser.fetch_int();
  if (parser.get_left_len() < size) {
    parser.set_error("Wrong set length");
    return;
  }
  s.clear();
  Key val;
  for (uint32 i = 0; i < size; i++) {
    parse(val, parser);
    s.insert(std::move(val));
  }
}

template <class T, class Storer>
std::enable_if_t<std::is_enum<T>::value> store(const T &val, Storer &storer) {
  store(static_cast<int32>(val), storer);
}
template <class T, class Parser>
std::enable_if_t<std::is_enum<T>::value> parse(T &val, Parser &parser) {
  int32 result;
  parse(result, parser);
  val = static_cast<T>(result);
}

template <class T, class Storer>
std::enable_if_t<!std::is_enum<T>::value> store(const T &val, Storer &storer) {
  val.store(storer);
}
template <class T, class Parser>
std::enable_if_t<!std::is_enum<T>::value> parse(T &val, Parser &parser) {
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
    TlStorerUnsafe storer(data.begin());
    store(object, storer);
    key.assign(data.begin(), data.size());
  } else {
    MutableSlice data = key;
    TlStorerUnsafe storer(data.begin());
    store(object, storer);
  }
  return key;
}

template <class T>
WARN_UNUSED_RESULT Status unserialize(T &object, Slice data) {
  TlParser parser(data);
  parse(object, parser);
  parser.fetch_end();
  return parser.get_status();
}
}  // namespace td
