#pragma once

namespace td {

struct Clocks {
  static double monotonic();

  static double system();

  static int tz_offset();
};

}  // namespace td
