#pragma once
#include "td/utils/port/config.h"

#include "td/utils/port/detail/ThreadStl.h"
#include "td/utils/port/detail/ThreadPthread.h"

namespace td {
#ifdef TD_THREAD_PTHREAD
using thread = detail::ThreadPthread;
#endif

#ifdef TD_THREAD_STL
using thread = detail::ThreadStl;
#endif
}  // namespace td
