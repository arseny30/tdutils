#pragma once

#include "td/utils/port/config.h"

#ifdef TD_POLL_EPOLL

// Epoll poll implementation
#include <sys/epoll.h>

#include "td/utils/port/Fd.h"
#include "td/utils/port/PollBase.h"

namespace td {
namespace detail {
class Epoll final : public PollBase {
 private:
  int epoll_fd = -1;
  vector<struct epoll_event> events;

 public:
  void init() override;

  void clear() override;

  void subscribe(const Fd &fd, Fd::Flags flags) override;

  void unsubscribe(const Fd &fd) override;

  void unsubscribe_before_close(const Fd &fd) override;

  void run(int timeout_ms) override;
};
}  // namespace detail
}  // namespace td

#endif  // TD_POLL_EPOLL
