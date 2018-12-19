#pragma once

#include <cstring>

namespace td {

template <class T>
class As {
 public:
  As(void *ptr) : ptr_(ptr) {
  }
  As(As &&) = default;
  As<T> &operator=(const As &new_value) && {
    memcpy(ptr_, new_value.ptr_, sizeof(T));
    return *this;
  }
  As<T> &operator=(const T new_value) && {
    memcpy(ptr_, &new_value, sizeof(T));
    return *this;
  }

  operator T() const {
    T res;
    memcpy(&res, ptr_, sizeof(T));
    return res;
  }

 private:
  void *ptr_;
};

template <class T>
class ConstAs {
 public:
  ConstAs(const void *ptr) : ptr_(ptr) {
  }

  operator T() const {
    T res;
    memcpy(&res, ptr_, sizeof(T));
    return res;
  }

 private:
  const void *ptr_;
};

template <class ToT, class FromT>
As<ToT> as(FromT *from) {
  return As<ToT>(from);
}

template <class ToT, class FromT>
const ConstAs<ToT> as(const FromT *from) {
  return ConstAs<ToT>(from);
}

}  // namespace td
