#pragma once
#include "td/utils/StorerBase.h"

#include "td/utils/Slice.h"
#include "td/utils/tl_storer.h"

#include <cstring>

namespace td {
class SliceStorer : public Storer {
 private:
  Slice slice;

 public:
  explicit SliceStorer(Slice slice) : slice(slice) {
  }
  size_t size() const override {
    return slice.size();
  }
  size_t store(uint8 *ptr) const override {
    memcpy(ptr, slice.ubegin(), slice.size());
    return slice.size();
  }
};

inline SliceStorer create_storer(Slice slice) {
  return SliceStorer(slice);
}

class ConcatStorer : public Storer {
 private:
  const Storer &a_;
  const Storer &b_;

 public:
  ConcatStorer(const Storer &a, const Storer &b) : a_(a), b_(b) {
  }

  size_t size() const override {
    return a_.size() + b_.size();
  }

  size_t store(uint8 *ptr) const override {
    uint8 *ptr_save = ptr;
    ptr += a_.store(ptr);
    ptr += b_.store(ptr);
    return ptr - ptr_save;
  }
};

inline ConcatStorer create_storer(const Storer &a, const Storer &b) {
  return ConcatStorer(a, b);
}

template <class T>
class DefaultStorer : public Storer {
 public:
  explicit DefaultStorer(const T &object) : object_(object) {
  }
  size_t size() const override {
    if (size_ == -1) {
      size_ = tl::calc_length(object_);
    }
    return size_;
  }
  size_t store(uint8 *ptr) const override {
    return tl::store_unsafe(object_, ptr);
  }

 private:
  mutable ssize_t size_ = -1;
  const T &object_;
};

template <class T>
DefaultStorer<T> create_default_storer(const T &from) {
  return DefaultStorer<T>(from);
}

}  // namespace td
