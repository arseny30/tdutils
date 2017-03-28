#pragma once
#include "td/utils/port/config.h"

#include "td/utils/port/detail/ThreadStl.h"
#include "td/utils/port/detail/ThreadPthread.h"

namespace td {
#ifdef TD_THREAD_PTHREAD
using thread = detail::ThreadPthread;
namespace this_thread = detail::this_thread_pthread;
#endif

#ifdef TD_THREAD_STL
using thread = detail::ThreadStl;
namespace this_thread = detail::this_thread_stl;
#endif
}  // namespace td
