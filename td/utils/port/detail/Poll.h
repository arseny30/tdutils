#pragma once

#include "td/utils/port/config.h"

#ifdef TD_POLL_POLL

// Poll poll implementation (finally)
#include <poll.h>

#include "td/utils/common.h"
#include "td/utils/port/PollBase.h"

namespace td {

namespace detail {
class Poll final : public PollBase {
 private:
  std::vector<struct pollfd> pollfds_;

 public:
  Poll() = default;
  Poll(const Poll &) = delete;
  Poll &operator=(const Poll &) = delete;
  Poll(Poll &&) = delete;
  Poll &operator=(Poll &&) = delete;
  ~Poll() override = default;

  void init() override;

  void clear() override;

  void subscribe(const Fd &fd, Fd::Flags flags) override;

  void unsubscribe(const Fd &fd) override;

  void unsubscribe_before_close(const Fd &fd) override;

  void run(int timeout_ms) override;
};
}  // namespace detail
}  // namespace td

#endif  // TD_POLL_KQUEUE
