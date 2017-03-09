#include "td/utils/Timer.h"

#include "td/utils/Slice.h"  // TODO move StringBuilder implementation to cpp, remove header

#include "td/utils/Time.h"
#include "td/utils/format.h"

namespace td {

Timer::Timer() : start_time_(Time::now()) {
}

StringBuilder &operator<<(StringBuilder &string_builder, const Timer &timer) {
  return string_builder << "in " << Time::now() - timer.start_time_;
}

PerfWarningTimer::PerfWarningTimer(string name, double max_duration)
    : name_(std::move(name)), start_at_(Time::now()), max_duration_(max_duration) {
}
PerfWarningTimer::~PerfWarningTimer() {
  if (start_at_ == 0) {
    return;
  }
  double duration = Time::now() - start_at_;
  LOG_IF(WARNING, duration > max_duration_) << "SLOW: " << tag("name", name_)
                                            << tag("duration", format::as_time(duration));
}

}  // namespace td
