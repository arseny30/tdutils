#pragma once

#include "td/utils/common.h"

#if TD_LINUX || TD_ANDROID
#define TD_POLL_EPOLL 1
#define TD_EVENTFD_POSIX 1
#define TD_PORT_POSIX 1
#elif TD_MAC
#define TD_POLL_KQUEUE 1
#define TD_EVENTFD_BSD 1
#define TD_PORT_POSIX 1
#elif TD_WINDOWS
#define TD_POLL_WINEVENT 1
#define TD_EVENTFD_WINDOWS 1
#define TD_PORT_WINDOWS 1
#endif
