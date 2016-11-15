#pragma once
#include "td/utils/port/config.h"

#ifdef TD_POLL_SELECT
#include "td/utils/port/Fd.h"
#include "td/utils/port/PollBase.h"

#include <sys/select.h>

namespace td {
namespace detail {
class Select final : public PollBase {
 public:
  void init() override;

  void clear() override;

  void subscribe(const Fd &fd, Fd::Flags flags) override;

  void unsubscribe(const Fd &fd) override;

  void unsubscribe_before_close(const Fd &fd) override;

  void run(int timeout_ms) override;

 private:
  struct FdInfo {
    Fd fd_ref;
    Fd::Flags flags;
  };
  std::vector<FdInfo> fds_;
  fd_set all_fd_;
  fd_set read_fd_;
  fd_set write_fd_;
  fd_set except_fd_;
  int max_fd_;
};
}  // end of namespace detail
}  // end of namespace td
#endif  // TD_POLL_SELECT
