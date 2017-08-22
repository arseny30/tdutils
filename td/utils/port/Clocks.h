#pragma once

#include <chrono>

namespace td {

class ClocksBase {
 public:
  using Duration = double;
};

// TODO: (maybe) write system specific functions.
class ClocksDefault {
 public:
  using Duration = ClocksBase::Duration;
  static Duration monotonic() {
    auto duration = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count()) * 1e-9;
  }
  static Duration system() {
    auto duration = std::chrono::system_clock::now().time_since_epoch();
    return static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count()) * 1e-9;
  }
};

using Clocks = ClocksDefault;

}  // namespace td
