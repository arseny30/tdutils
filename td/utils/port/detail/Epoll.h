#pragma once

#include "td/utils/port/config.h"

#ifdef TD_POLL_EPOLL

#include "td/utils/common.h"
#include "td/utils/port/Fd.h"
#include "td/utils/port/PollBase.h"

#include <sys/epoll.h>

namespace td {
namespace detail {

class Epoll final : public PollBase {
 public:
  Epoll() = default;
  Epoll(const Epoll &) = delete;
  Epoll &operator=(const Epoll &) = delete;
  Epoll(Epoll &&) = delete;
  Epoll &operator=(Epoll &&) = delete;
  ~Epoll() override = default;

  void init() override;

  void clear() override;

  void subscribe(const Fd &fd, Fd::Flags flags) override;

  void unsubscribe(const Fd &fd) override;

  void unsubscribe_before_close(const Fd &fd) override;

  void run(int timeout_ms) override;

 private:
  int epoll_fd = -1;
  vector<struct epoll_event> events;
};

}  // namespace detail
}  // namespace td

#endif
