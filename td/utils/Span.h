#pragma once

#include "td/utils/common.h"
#include "td/utils/logging.h"

#include <array>

namespace td {

namespace detail {
template <class T, class InnerT>
class SpanImpl {
  InnerT *data_{nullptr};
  size_t size_{0};

 public:
  SpanImpl() = default;
  SpanImpl(InnerT *data, size_t size) : data_(data), size_(size) {
  }
  SpanImpl(InnerT &data) : SpanImpl(&data, 1) {
  }

  template <class OtherInnerT>
  SpanImpl(const SpanImpl<T, OtherInnerT> &other) : SpanImpl(other.data(), other.size()) {
  }

  template <size_t N>
  SpanImpl(const std::array<T, N> &arr) : SpanImpl(arr.data(), arr.size()) {
  }
  template <size_t N>
  SpanImpl(std::array<T, N> &arr) : SpanImpl(arr.data(), arr.size()) {
  }
  SpanImpl(const vector<T> &v) : SpanImpl(v.data(), v.size()) {
  }
  SpanImpl(vector<T> &v) : SpanImpl(v.data(), v.size()) {
  }

  template <class OtherInnerT>
  SpanImpl &operator=(const SpanImpl<T, OtherInnerT> &other) {
    SpanImpl copy{other};
    *this = copy;
  }

  InnerT &operator[](size_t i) {
    DCHECK(i < size());
    return data_[i];
  }

  InnerT *data() const {
    return data_;
  }
  InnerT *begin() const {
    return data_;
  }
  InnerT *end() const {
    return data_ + size_;
  }
  size_t size() const {
    return size_;
  }

  SpanImpl &truncate(size_t size) {
    CHECK(size <= size_);
    size_ = size;
    return *this;
  }

  SpanImpl substr(size_t offset) const {
    CHECK(offset <= size_);
    return SpanImpl(begin() + offset, size_ - offset);
  }
};
}  // namespace detail

template <class T>
using Span = detail::SpanImpl<T, const T>;

template <class T>
using MutableSpan = detail::SpanImpl<T, T>;

}  // namespace td
