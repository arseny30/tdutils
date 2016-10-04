#include "td/utils/Timer.h"

#include "td/utils/Slice.h"  // TODO move StringBuilder implementation to cpp, remove header
#include "td/utils/Time.h"

namespace td {

Timer::Timer() : start_time_(Time::now()) {
}

StringBuilder &operator<<(StringBuilder &string_builder, const Timer &timer) {
  return string_builder << "in " << Time::now() - timer.start_time_;
}

}  // end of namespace td
