#pragma once
#include "td/utils/Slice.h"

namespace td {
class Storer {
 public:
  virtual size_t size() const = 0;
  virtual size_t store(uint8 *ptr) const = 0;
};

class SliceStorer : public Storer {
 private:
  Slice slice;

 public:
  explicit SliceStorer(const Slice &slice) : slice(slice) {
  }
  size_t size() const override {
    return slice.size();
  }
  size_t store(uint8 *ptr) const override {
    memcpy(ptr, slice.ubegin(), slice.size());
    return slice.size();
  }
};

inline SliceStorer create_storer(const Slice &slice) {
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
}  // end of namespace td
