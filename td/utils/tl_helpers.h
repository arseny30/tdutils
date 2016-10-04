#pragma once

#include "td/tl/tl_storer.h"
#include "td/tl/tl_parser.h"

namespace td {
template <class T, class Storer>
inline void store(const T &val, Storer &storer) {
  val.store(storer);
}
template <class T, class Parser>
inline void parse(T &val, Parser &parser) {
  val.parse(parser);
}

template <class StorerT>
inline void store(const Storer &x, StorerT &storer) {
  storer.store_storer(x);
}

template <class Storer>
inline void store(double x, Storer &storer) {
  storer.template store_binary<double>(x);
}
template <class Parser>
inline void parse(double &x, Parser &parser) {
  x = parser.fetch_double();
}

template <class T, class Storer>
inline void store(const vector<T> &vec, Storer &storer) {
  storer.template store_binary<int32>(static_cast<int32>(vec.size()));
  for (auto &val : vec) {
    store(val, storer);
  }
}

template <class T, class Parser>
inline void parse(vector<T> &vec, Parser &parser) {
  int32 size = parser.fetch_int();
  vec = vector<T>(size);
  for (auto &val : vec) {
    parse(val, parser);
  }
}

template <class T>
inline string serialize(const T &object) {
  tl::TlStorer storer;
  store(object, storer);
  return storer.move_as<string>();
}

template <class T>
inline WARN_UNUSED_RESULT Status unserialize(T &object, Slice data) {
  StackAllocator<>::Ptr ptr;
  if ((reinterpret_cast<uint64>(data.begin()) & 3) != 0) {  // not aligned
    ptr = StackAllocator<>::alloc(data.size());
    memcpy(ptr.get(), data.begin(), data.size());
    data = ptr.as_slice();
  }
  tl::tl_parser parser(data.begin(), data.size());
  parse(object, parser);
  parser.fetch_end();
  if (parser.get_error()) {
    return Status::Error(parser.get_error());
  }
  return Status::OK();
}
}
