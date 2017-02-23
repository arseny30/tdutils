#pragma once

#include "td/utils/StringBuilder.h"

namespace td {

class Timer {
 public:
  Timer();

 private:
  friend StringBuilder &operator<<(StringBuilder &string_builder, const Timer &timer);

  double start_time_;
};

}  // namespace td
