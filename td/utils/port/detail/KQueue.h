#pragma once

#include "td/utils/port/config.h"

#ifdef TD_POLL_KQUEUE

#include "td/utils/common.h"
#include "td/utils/List.h"
#include "td/utils/port/detail/PollableFd.h"
#include "td/utils/port/PollBase.h"
#include "td/utils/port/PollFlags.h"

#include <cstdint>

#include <sys/event.h>

namespace td {
namespace detail {

class KQueue final : public PollBase {
 public:
  KQueue();
  KQueue(const KQueue &) = delete;
  KQueue &operator=(const KQueue &) = delete;
  KQueue(KQueue &&) = delete;
  KQueue &operator=(KQueue &&) = delete;
  ~KQueue() override;

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
  vector<struct kevent> events;
  int changes_n;
  int kq;
  ListNode list_root;

  int update(int nevents, const timespec *timeout, bool may_fail = false);

  void invalidate(int native_fd);

  void flush_changes(bool may_fail = false);

  void add_change(std::uintptr_t ident, int16 filter, uint16 flags, uint32 fflags, std::intptr_t data, void *udata);
};

}  // namespace detail
}  // namespace td

#endif
