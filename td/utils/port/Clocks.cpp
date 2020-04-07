#include "td/utils/port/Clocks.h"

#include <chrono>

namespace td {

double Clocks::monotonic() {
  // TODO write system specific functions, because std::chrono::steady_clock is steady only under Windows
  auto duration = std::chrono::steady_clock::now().time_since_epoch();
  return static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count()) * 1e-9;
}

double Clocks::system() {
  auto duration = std::chrono::system_clock::now().time_since_epoch();
  return static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count()) * 1e-9;
}

}  // namespace td
