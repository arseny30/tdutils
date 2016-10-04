#pragma once

#include <cstdarg>

#include "td/utils/common.h"
#include "td/utils/Slice-decl.h"
#include "td/utils/StackAllocator.h"

namespace td {
/*** StringBuilder ***/

class StringBuilder {
 public:
  explicit StringBuilder(const MutableSlice &slice)
      : begin_ptr_(slice.begin()), current_ptr_(begin_ptr_), end_ptr_(slice.end() - reserved_size) {
    ASSERT_CHECK(slice.size() > reserved_size);
  }

  void clear() {
    current_ptr_ = begin_ptr_;
    error_flag_ = false;
  }
  MutableCSlice as_slice() {
    ASSERT_CHECK(current_ptr_ < end_ptr_ + reserved_size);
    *current_ptr_ = 0;
    return MutableCSlice(begin_ptr_, current_ptr_);
  }

  bool is_error() {
    return error_flag_;
  }

  StringBuilder &operator<<(const char *slice) {
    return *this << Slice(slice);
  }

  StringBuilder &operator<<(const Slice &slice) {
    char *next_ptr = current_ptr_ + slice.size();
    size_t size = slice.size();
    if (unlikely(next_ptr > end_ptr_)) {
      if (unlikely(end_ptr_ < current_ptr_)) {
        return on_error();
      }
      size = end_ptr_ - current_ptr_;
      next_ptr = end_ptr_;
    }
    memcpy(current_ptr_, slice.begin(), size);
    current_ptr_ = next_ptr;
    return *this;
  }

  StringBuilder &operator<<(bool b) {
    if (b) {
      return *this << "true";
    } else {
      return *this << "false";
    }
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

  // TODO: optimize
  StringBuilder &operator<<(int x) {
    if (unlikely(end_ptr_ < current_ptr_)) {
      return on_error();
    }
    current_ptr_ += snprintf(current_ptr_, reserved_size, "%d", x);
    return *this;
  }

  StringBuilder &operator<<(unsigned int x) {
    if (unlikely(end_ptr_ < current_ptr_)) {
      return on_error();
    }
    current_ptr_ += snprintf(current_ptr_, reserved_size, "%u", x);
    return *this;
  }

  StringBuilder &operator<<(long int x) {
    if (unlikely(end_ptr_ < current_ptr_)) {
      return on_error();
    }
    current_ptr_ += snprintf(current_ptr_, reserved_size, "%ld", x);
    return *this;
  }

  StringBuilder &operator<<(long unsigned int x) {
    if (unlikely(end_ptr_ < current_ptr_)) {
      return on_error();
    }
    current_ptr_ += snprintf(current_ptr_, reserved_size, "%lu", x);
    return *this;
  }

  StringBuilder &operator<<(long long int x) {
    if (unlikely(end_ptr_ < current_ptr_)) {
      return on_error();
    }
    current_ptr_ += snprintf(current_ptr_, reserved_size, "%lld", x);
    return *this;
  }

  StringBuilder &operator<<(long long unsigned int x) {
    if (unlikely(end_ptr_ < current_ptr_)) {
      return on_error();
    }
    current_ptr_ += snprintf(current_ptr_, reserved_size, "%llu", x);
    return *this;
  }

  StringBuilder &operator<<(double x) {
    if (unlikely(end_ptr_ < current_ptr_)) {
      return on_error();
    }
    current_ptr_ += snprintf(current_ptr_, reserved_size, "%lf", x);
    return *this;
  }

  StringBuilder &operator<<(const void *ptr) {
    if (unlikely(end_ptr_ < current_ptr_)) {
      return on_error();
    }
    current_ptr_ += snprintf(current_ptr_, reserved_size, "%p", ptr);
    return *this;
  }

  void vprintf(const char *fmt, va_list list) {
    if (unlikely(end_ptr_ <= current_ptr_)) {
      on_error();
      return;
    }
    int len = vsnprintf(current_ptr_, end_ptr_ - current_ptr_ - 1, fmt, list);
    char *next_ptr = current_ptr_ + len;
    if (likely(next_ptr <= end_ptr_)) {
      current_ptr_ = next_ptr;
    } else {
      current_ptr_ = end_ptr_;
    }
  }

  void printf(const char *fmt, ...) IF_NO_MSVC(__attribute__((format(printf, 2, 3)))) {
    va_list list;
    va_start(list, fmt);
    vprintf(fmt, list);
    va_end(list);
  }

 private:
  char *begin_ptr_;
  char *current_ptr_;
  char *end_ptr_;
  bool error_flag_ = false;
  enum { reserved_size = 30 };

  StringBuilder &on_error() {
    error_flag_ = true;
    return *this;
  }
};

template <class T>
string to_string(const T &x) {
  const size_t buf_size = 8 * 1024;
  auto buf = StackAllocator<>::alloc(buf_size);
  StringBuilder sb(buf.as_slice());
  sb << x;
  return sb.as_slice().str();
}

}  // end of namespace td;
