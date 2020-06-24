#include "td/utils/Time.h"

#include <cmath>
#include <atomic>

namespace td {

bool operator==(Timestamp a, Timestamp b) {
  return std::abs(a.at() - b.at()) < 1e-6;
}
namespace {
std::atomic<double> time_diff;
}
double Time::now() {
  return now_unadjusted() + time_diff.load(std::memory_order_relaxed);
}

double Time::now_unadjusted() {
  return Clocks::monotonic();
}

void Time::jump_in_future(double at) {
  auto old_time_diff = time_diff.load();

  while (true) {
    auto diff = at - now();
    if (diff < 0) {
      return;
    }
    if (time_diff.compare_exchange_strong(old_time_diff, old_time_diff + diff)) {
      return;
    }
  }
}

}  // namespace td
