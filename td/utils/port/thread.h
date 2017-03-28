#pragma once

#include "td/utils/port/config.h"

#include "td/utils/port/detail/ThreadStl.h"
#include "td/utils/port/detail/ThreadPthread.h"

namespace td {
#if TD_THREAD_PTHREAD
using thread = detail::ThreadPthread;
namespace this_thread = detail::this_thread_pthread;
#elif TD_THREAD_STL
using thread = detail::ThreadStl;
namespace this_thread = detail::this_thread_stl;
#else
#error "Thread's implementation is not defined"
#endif
}  // namespace td
