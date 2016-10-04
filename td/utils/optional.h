#pragma once
#include "td/utils/Status.h"
namespace td {
template <class T>
class optional {
 public:
  optional() = default;
  optional(T &&t) : impl_(std::forward<T>(t)) {
  }
  operator bool() {
    return impl_.is_ok();
  }
  T &value() {
    return impl_.ok_ref();
  }
  T &operator*() {
    return value();
  }

 private:
  Result<T> impl_;
};
}  // namespace td
