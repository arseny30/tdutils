#pragma once

#include <atomic>

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
  if (new_timeout == 0) {  //-V550
    return;
  }
  if (*timeout == 0 || new_timeout < *timeout) {  //-V550
    *timeout = new_timeout;
  }
}

}  // end of namespace td
