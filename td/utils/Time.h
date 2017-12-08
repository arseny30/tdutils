#pragma once

#include <atomic>
#include <cmath>

#include "td/utils/common.h"
#include "td/utils/port/Clocks.h"

namespace td {

class Time {
 public:
  static double now() {
    double now = Clocks::monotonic();
    now_.store(now, std::memory_order_relaxed);
    return now;
  }
  static double now_cached() {
    return now_.load(std::memory_order_relaxed);
  }

 private:
  static std::atomic<double> now_;
};

inline void relax_timeout_at(double *timeout, double new_timeout) {
  if (new_timeout == 0) {
    return;
  }
  if (*timeout == 0 || new_timeout < *timeout) {
    *timeout = new_timeout;
  }
}

class Timestamp {
 public:
  Timestamp() = default;
  static Timestamp never() {
    return Timestamp{};
  }
  static Timestamp now() {
    return Timestamp{Time::now()};
  }
  static Timestamp now_cached() {
    return Timestamp{Time::now_cached()};
  }
  static Timestamp at(double timeout) {
    return Timestamp{timeout};
  }

  static Timestamp in(double timeout) {
    return Timestamp{Time::now_cached() + timeout};
  }

  bool is_in_past() const {
    return at_ <= Time::now_cached();
  }

  explicit operator bool() const {
    return at_ > 0;
  }

  double at() const {
    return at_;
  }

  double in() const {
    return at_ - Time::now_cached();
  }

  void relax(const Timestamp &timeout) {
    if (!timeout) {
      return;
    }
    if (!*this || at_ > timeout.at_) {
      at_ = timeout.at_;
    }
  }

  friend bool operator==(Timestamp a, Timestamp b) {
    return std::abs(a.at() - b.at()) < 1e-6;
  }

 private:
  double at_{0};

  explicit Timestamp(double timeout) : at_(timeout) {
  }
};

}  // namespace td
