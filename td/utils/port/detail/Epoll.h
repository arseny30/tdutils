#pragma once

#include "td/utils/port/config.h"

#ifdef TD_POLL_EPOLL

#include "td/utils/common.h"
#include "td/utils/List.h"
#include "td/utils/port/detail/PollableFd.h"
#include "td/utils/port/PollBase.h"
#include "td/utils/port/PollFlags.h"

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

  void subscribe(PollableFd fd, PollFlags flags) override;

  void unsubscribe(PollableFdRef fd) override;

  void unsubscribe_before_close(PollableFdRef fd) override;

  void run(int timeout_ms) override;

  static bool is_edge_triggered() {
    return true;
  }

 private:
  int epoll_fd = -1;
  vector<struct epoll_event> events;
  ListNode list_root;
};

}  // namespace detail
}  // namespace td

#endif
