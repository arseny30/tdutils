#pragma once
#include "td/utils/port/config.h"
// include both and let config.h decide
#include "td/utils/port/detail/EventFdBsd.h"
#include "td/utils/port/detail/EventFdPosix.h"
#include "td/utils/port/detail/EventFdWindows.h"

namespace td {
#ifdef TD_EVENTFD_BSD
using EventFd = detail::EventFdBsd;
#endif  // TD_EVENTFD_BSD

#ifdef TD_EVENTFD_POSIX
using EventFd = detail::EventFdPosix;
#endif  // TD_EVENTFD_POSIX

#ifdef TD_EVENTFD_WINDOWS
using EventFd = detail::EventFdWindows;
#endif  // TD_EVENTFD_WINDOWS
}  // namespace td
