#include "td/utils/Time.h"

#include <cmath>

namespace td {

std::atomic<double> Time::now_;

bool operator==(Timestamp a, Timestamp b) {
  return std::abs(a.at() - b.at()) < 1e-6;
}

}  // namespace td
