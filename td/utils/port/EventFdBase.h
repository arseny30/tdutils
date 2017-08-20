#pragma once

#include "td/utils/common.h"
#include "td/utils/port/Fd.h"
#include "td/utils/Status.h"

namespace td {
class EventFdBase {
 public:
  EventFdBase() = default;
  EventFdBase(const EventFdBase &) = delete;
  EventFdBase &operator=(const EventFdBase &) = delete;
  EventFdBase(EventFdBase &&) = default;
  EventFdBase &operator=(EventFdBase &&) = default;
  virtual ~EventFdBase() = default;

  virtual void init() = 0;
  virtual bool empty() = 0;
  virtual void close() = 0;
  virtual const Fd &get_fd() const = 0;
  virtual Fd &get_fd() = 0;
  virtual Status get_pending_error() WARN_UNUSED_RESULT = 0;
  virtual void release() = 0;
  virtual void acquire() = 0;
};
}  // namespace td
