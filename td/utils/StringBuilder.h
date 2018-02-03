#pragma once

#include "td/utils/common.h"
#include "td/utils/Slice-decl.h"
#include "td/utils/StackAllocator.h"

#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <type_traits>

namespace td {

class StringBuilder {
 public:
  explicit StringBuilder(MutableSlice slice)
      : begin_ptr_(slice.begin()), current_ptr_(begin_ptr_), end_ptr_(slice.end() - reserved_size) {
    if (slice.size() <= reserved_size) {
      std::abort();  // shouldn't happen
    }
  }

  void clear() {
    current_ptr_ = begin_ptr_;
    error_flag_ = false;
  }

  MutableCSlice as_cslice() {
    if (current_ptr_ >= end_ptr_ + reserved_size) {
      std::abort();  // shouldn't happen
    }
    *current_ptr_ = 0;
    return MutableCSlice(begin_ptr_, current_ptr_);
  }

  bool is_error() const {
    return error_flag_;
  }

  StringBuilder &operator<<(const char *str) {
    return *this << Slice(str);
  }

  StringBuilder &operator<<(Slice slice) {
    if (unlikely(end_ptr_ < current_ptr_)) {
      return on_error();
    }
    auto size = static_cast<size_t>(end_ptr_ + reserved_size - 1 - current_ptr_);
    if (unlikely(slice.size() > size)) {
      error_flag_ = true;
    } else {
      size = slice.size();
    }
    std::memcpy(current_ptr_, slice.begin(), size);
    current_ptr_ += size;
    return *this;
  }

  StringBuilder &operator<<(bool b) {
    return *this << (b ? Slice("true") : Slice("false"));
  }

  StringBuilder &operator<<(char c) {
    if (unlikely(end_ptr_ < current_ptr_)) {
      return on_error();
    }
    *current_ptr_++ = c;
    return *this;
  }

  StringBuilder &operator<<(unsigned char c) {
    return *this << static_cast<unsigned int>(c);
  }

  StringBuilder &operator<<(signed char c) {
    return *this << static_cast<int>(c);
  }

  StringBuilder &operator<<(int x);

  StringBuilder &operator<<(unsigned int x);

  StringBuilder &operator<<(long int x);

  StringBuilder &operator<<(long unsigned int x);

  StringBuilder &operator<<(long long int x);

  StringBuilder &operator<<(long long unsigned int x);

  StringBuilder &operator<<(double x);

  StringBuilder &operator<<(const void *ptr);

  template <class T>
  StringBuilder &operator<<(const T *ptr) {
    return *this << static_cast<const void *>(ptr);
  }

  void vprintf(const char *fmt, va_list list);

  void printf(const char *fmt, ...) TD_ATTRIBUTE_FORMAT_PRINTF(2, 3);

 private:
  char *begin_ptr_;
  char *current_ptr_;
  char *end_ptr_;
  bool error_flag_ = false;
  static constexpr size_t reserved_size = 30;

  StringBuilder &on_error() {
    error_flag_ = true;
    return *this;
  }
};

template <class T>
std::enable_if_t<std::is_arithmetic<T>::value, string> to_string(const T &x) {
  const size_t buf_size = 1000;
  auto buf = StackAllocator::alloc(buf_size);
  StringBuilder sb(buf.as_slice());
  sb << x;
  return sb.as_cslice().str();
}

}  // namespace td
