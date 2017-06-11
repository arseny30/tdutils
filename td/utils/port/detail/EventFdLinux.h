#pragma once

#include "td/utils/port/config.h"

#ifdef TD_EVENTFD_LINUX

#include "td/utils/port/EventFdBase.h"

namespace td {
namespace detail {
class EventFdLinux final : public EventFdBase {
 private:
  Fd fd_;

 public:
  explicit operator FdRef() override;

  void init() override;

  bool empty() override;

  void close() override;

  Status get_pending_error() override WARN_UNUSED_RESULT;

  const Fd &get_fd() const override;
  Fd &get_fd() override;

  void release() override;

  void acquire() override;
};
}  // namespace detail
}  // namespace td

#endif  // TD_EVENTFD_LINUX
