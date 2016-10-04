#pragma once
#include <chrono>

namespace td {
class ClocksBase {
 public:
  using Duration = double;
  // Duration monotonic() = 0;
  // Duration system() = 0;
};

// TODO: (may be) write system specific functions.
class ClocksDefault {
 public:
  using Duration = ClocksBase::Duration;
  static Duration monotonic() {
    using namespace std::chrono;
    return static_cast<double>(duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count()) * 1e-9;
  }
  static Duration system() {
    using namespace std::chrono;
    return static_cast<double>(duration_cast<nanoseconds>(system_clock::now().time_since_epoch()).count()) * 1e-9;
  }
};

using Clocks = ClocksDefault;
}  // end of namespace td
