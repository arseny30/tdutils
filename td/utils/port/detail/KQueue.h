#pragma once

#include "td/utils/port/config.h"

#ifdef TD_POLL_KQUEUE

// KQueue poll implementation
#include <sys/event.h>

#include "td/utils/port/PollBase.h"

namespace td {
namespace detail {
class KQueue final : public PollBase {
 private:
  vector<struct kevent> events;
  int changes_n;
  int kq;

  int update(int nevents, const struct timespec *timeout);

  void invalidate(const Fd &fd);

  void flush_changes();

  void add_change(uintptr_t ident, int16 filter, uint16 flags, uint32 fflags, intptr_t data, void *udata);

 public:
  KQueue();
  KQueue(const KQueue &) = delete;
  KQueue &operator=(const KQueue &) = delete;
  KQueue(KQueue &&) = delete;
  KQueue &operator=(KQueue &&) = delete;
  ~KQueue();

  void init() override;

  void clear() override;

  void subscribe(const Fd &fd, Fd::Flags flags = Fd::Write | Fd::Read) override;

  void unsubscribe(const Fd &fd) override;

  void unsubscribe_before_close(const Fd &fd) override;

  void run(int timeout_ms) override;
};
}  // end of namespace detail
}  // end of namespace td

#endif  // TD_POLL_KQUEUE
