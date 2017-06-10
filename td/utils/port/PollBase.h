#pragma once

#include "td/utils/port/Fd.h"

namespace td {
class PollBase {
 public:
  PollBase() = default;
  PollBase(const PollBase &) = delete;
  PollBase &operator=(const PollBase &) = delete;
  PollBase(PollBase &&) = default;
  PollBase &operator=(PollBase &&) = default;
  virtual ~PollBase() = default;
  virtual void init() = 0;
  virtual void clear() = 0;
  virtual void subscribe(const Fd &fd, Fd::Flags flags) = 0;
  virtual void unsubscribe(const Fd &fd) = 0;
  virtual void unsubscribe_before_close(const Fd &fd) = 0;
  virtual void run(int timeout_ms) = 0;
};
}  // namespace td
