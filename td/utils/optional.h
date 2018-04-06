#pragma once

#include "td/utils/Status.h"

#include <utility>

namespace td {

template <class T>
class optional {
 public:
  optional() = default;
  template <class T1>
  optional(T1 &&t) : impl_(std::forward<T1>(t)) {
  }
  explicit operator bool() const {
    return impl_.is_ok();
  }
  T &value() {
    return impl_.ok_ref();
  }
  const T &value() const {
    return impl_.ok_ref();
  }
  T &operator*() {
    return value();
  }

 private:
  Result<T> impl_;
};

}  // namespace td
