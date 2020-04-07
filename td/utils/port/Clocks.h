#pragma once

namespace td {

struct Clocks {
  static double monotonic();

  static double system();
};

}  // namespace td
