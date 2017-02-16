#pragma once

#include "td/utils/port/Fd.h"

namespace td {
class PollBase {
 public:
  virtual ~PollBase() = default;
  virtual void init() = 0;
  virtual void clear() = 0;
  virtual void subscribe(const Fd &fd, Fd::Flags flags) = 0;
  virtual void unsubscribe(const Fd &fd) = 0;
  virtual void unsubscribe_before_close(const Fd &fd) = 0;
  virtual void run(int timeout_ms) = 0;
};
}  // end of namespace td
