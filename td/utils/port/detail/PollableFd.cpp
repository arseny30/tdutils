#include "td/utils/port/detail/PollableFd.h"

#if TD_POSIX_PORT
#include <unistd.h>
#endif

namespace td {
bool PollFlagsSet::write_flags(PollFlags flags) {
  if (flags.empty()) {
    return false;
  }
  auto old_flags = to_write_.fetch_or(flags.raw(), std::memory_order_relaxed);
  return (flags.raw() & ~old_flags) != 0;
}
bool PollFlagsSet::write_flags_local(PollFlags flags) {
  return flags_.add_flags(flags);
}
bool PollFlagsSet::flush() const {
  if (to_write_.load(std::memory_order_relaxed) == 0) {
    return false;
  }
  auto to_write = to_write_.exchange(0, std::memory_order_relaxed);
  auto old_flags = flags_;
  flags_.add_flags(PollFlags::from_raw(to_write));
  if (flags_.can_close()) {
    flags_.remove_flags(PollFlags::Write());
  }
  return flags_ != old_flags;
}
PollFlags PollFlagsSet::read_flags() const {
  flush();
  return flags_;
}
PollFlags PollFlagsSet::read_flags_local() const {
  return flags_;
}
void PollFlagsSet::clear_flags(PollFlags flags) {
  flags_.remove_flags(flags);
}
void PollFlagsSet::clear() {
  to_write_ = 0;
  flags_ = {};
}

}  // namespace td
