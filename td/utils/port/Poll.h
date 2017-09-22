#pragma once

#include "td/utils/port/config.h"

#include "td/utils/port/detail/Epoll.h"
#include "td/utils/port/detail/KQueue.h"
#include "td/utils/port/detail/Poll.h"
#include "td/utils/port/detail/Select.h"
#include "td/utils/port/detail/WineventPoll.h"

namespace td {
#ifdef TD_POLL_SELECT
using Poll = detail::Select;
#endif  // TD_POLL_SELECT

#ifdef TD_POLL_POLL
using Poll = detail::Poll;
#endif  // TD_POLL_POLL

#ifdef TD_POLL_EPOLL
using Poll = detail::Epoll;
#endif  // TD_POLL_EPOLL

#ifdef TD_POLL_KQUEUE
using Poll = detail::KQueue;
#endif  // TD_POLL_KQUEUE

#ifdef TD_POLL_WINEVENT
using Poll = detail::WineventPoll;
#endif
}  // namespace td
