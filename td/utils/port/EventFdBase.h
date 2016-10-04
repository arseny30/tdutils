#pragma once

#include "td/utils/port/Fd.h"

namespace td {
class EventFdBase {
 public:
  virtual operator FdRef() = 0;
  virtual void init() = 0;
  virtual bool empty() = 0;
  virtual void close() = 0;
  virtual const Fd &get_fd() const = 0;
  virtual Fd &get_fd() = 0;
  virtual Status get_pending_error() WARN_UNUSED_RESULT = 0;
  virtual void release() = 0;
  virtual void acquire() = 0;
};
}  // end of namespace td
