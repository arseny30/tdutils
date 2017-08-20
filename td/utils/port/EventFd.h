#pragma once

#include "td/utils/port/config.h"

// include all and let config.h decide
#include "td/utils/port/detail/EventFdBsd.h"
#include "td/utils/port/detail/EventFdLinux.h"
#include "td/utils/port/detail/EventFdWindows.h"

namespace td {
#ifdef TD_EVENTFD_BSD
using EventFd = detail::EventFdBsd;
#endif  // TD_EVENTFD_BSD

#ifdef TD_EVENTFD_LINUX
using EventFd = detail::EventFdLinux;
#endif  // TD_EVENTFD_LINUX

#ifdef TD_EVENTFD_WINDOWS
using EventFd = detail::EventFdWindows;
#endif  // TD_EVENTFD_WINDOWS
}  // namespace td
