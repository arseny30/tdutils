#pragma once

#include "td/utils/port/config.h"

#ifdef TD_EVENTFD_BSD

#include "td/utils/port/EventFdBase.h"

namespace td {
namespace detail {
class EventFdBsd final : public EventFdBase {
 private:
  Fd in_;
  Fd out_;

 public:
  EventFdBsd() = default;
  EventFdBsd(const EventFdBsd &) = delete;
  EventFdBsd &operator=(const EventFdBsd &) = delete;
  EventFdBsd(EventFdBsd &&from) = default;
  EventFdBsd &operator=(EventFdBsd &&from) = default;
  operator FdRef() override;

  void init() override;

  bool empty() override;

  void close() override;

  Status get_pending_error() override WARN_UNUSED_RESULT;

  const Fd &get_fd() const override;
  Fd &get_fd() override;

  void release() override;

  void acquire() override;
};
}  // end of namespace detail
}  // end of namespace td

#endif  // TD_EVENTFD_BSD
