#include "td/utils/StringBuilder.h"

#include <cstdio>

namespace td {

// TODO: optimize
StringBuilder &StringBuilder::operator<<(int x) {
  if (unlikely(end_ptr_ < current_ptr_)) {
    return on_error();
  }
  current_ptr_ += std::snprintf(current_ptr_, reserved_size, "%d", x);
  return *this;
}

StringBuilder &StringBuilder::operator<<(unsigned int x) {
  if (unlikely(end_ptr_ < current_ptr_)) {
    return on_error();
  }
  current_ptr_ += std::snprintf(current_ptr_, reserved_size, "%u", x);
  return *this;
}

StringBuilder &StringBuilder::operator<<(long int x) {
  if (unlikely(end_ptr_ < current_ptr_)) {
    return on_error();
  }
  current_ptr_ += std::snprintf(current_ptr_, reserved_size, "%ld", x);
  return *this;
}

StringBuilder &StringBuilder::operator<<(long unsigned int x) {
  if (unlikely(end_ptr_ < current_ptr_)) {
    return on_error();
  }
  current_ptr_ += std::snprintf(current_ptr_, reserved_size, "%lu", x);
  return *this;
}

StringBuilder &StringBuilder::operator<<(long long int x) {
  if (unlikely(end_ptr_ < current_ptr_)) {
    return on_error();
  }
  current_ptr_ += std::snprintf(current_ptr_, reserved_size, "%lld", x);
  return *this;
}

StringBuilder &StringBuilder::operator<<(long long unsigned int x) {
  if (unlikely(end_ptr_ < current_ptr_)) {
    return on_error();
  }
  current_ptr_ += std::snprintf(current_ptr_, reserved_size, "%llu", x);
  return *this;
}

StringBuilder &StringBuilder::operator<<(double x) {
  if (unlikely(end_ptr_ < current_ptr_)) {
    return on_error();
  }
  auto left = end_ptr_ + reserved_size - current_ptr_;
  int len = std::snprintf(current_ptr_, left, "%lf", x);
  if (unlikely(len >= left)) {
    error_flag_ = true;
    current_ptr_ += left - 1;
  } else {
    current_ptr_ += len;
  }
  return *this;
}

StringBuilder &StringBuilder::operator<<(const void *ptr) {
  if (unlikely(end_ptr_ < current_ptr_)) {
    return on_error();
  }
  current_ptr_ += std::snprintf(current_ptr_, reserved_size, "%p", ptr);
  return *this;
}

void StringBuilder::vprintf(const char *fmt, va_list list) {
  if (unlikely(end_ptr_ < current_ptr_)) {
    on_error();
    return;
  }

  auto left = end_ptr_ + reserved_size - current_ptr_;
  int len = std::vsnprintf(current_ptr_, left, fmt, list);
  if (unlikely(len >= left)) {
    error_flag_ = true;
    current_ptr_ += left - 1;
  } else {
    current_ptr_ += len;
  }
}

void StringBuilder::printf(const char *fmt, ...) {
  va_list list;
  va_start(list, fmt);
  vprintf(fmt, list);
  va_end(list);
}

}  // namespace td
