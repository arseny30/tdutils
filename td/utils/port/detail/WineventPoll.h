#pragma once

#include "td/utils/port/config.h"

#ifdef TD_POLL_WINEVENT
#include "td/utils/misc.h"
#include "td/utils/port/PollBase.h"

namespace td {
namespace detail {
class WineventPoll final : public PollBase {
 public:
  void init() override {
    clear();
  }

  void clear() override {
    fds_.clear();
  }

  void subscribe(const Fd &fd, Fd::Flags flags) override {
    for (auto &it : fds_) {
      if (it.fd_ref.get_key() == fd.get_key()) {
        it.flags = flags;
        return;
      }
    }
    fds_.push_back({fd.clone(), flags});
  }

  void unsubscribe(const Fd &fd) override {
    for (auto it = fds_.begin(); it != fds_.end(); it++) {
      if (it->fd_ref.get_key() == fd.get_key()) {
        std::swap(*it, fds_.back());
        fds_.pop_back();
        return;
      }
    }
  }

  void unsubscribe_before_close(const Fd &fd) override {
    unsubscribe(fd);
  }

  void run(int timeout_ms) override {
    std::vector<std::pair<size_t, Fd::Flag>> events_desc;
    std::vector<HANDLE> events;
    for (size_t i = 0; i < fds_.size(); i++) {
      auto &fd_info = fds_[i];
      if (fd_info.flags & Fd::Flag::Write) {
        events_desc.emplace_back(i, Fd::Flag::Write);
        events.push_back(fd_info.fd_ref.get_write_event());
      }
      if (fd_info.flags & Fd::Flag::Read) {
        events_desc.emplace_back(i, Fd::Flag::Read);
        events.push_back(fd_info.fd_ref.get_read_event());
      }
    }
    auto status = WaitForMultipleObjects(narrow_cast<DWORD>(events.size()), events.data(), false, timeout_ms);
    LOG_IF(FATAL, status == WAIT_FAILED) << Status::OsError("WaitforMultipleObjects failed");
    for (size_t i = 0; i < events.size(); i++) {
      if (WaitForSingleObject(events[i], 0) == WAIT_OBJECT_0) {
        auto &fd = fds_[events_desc[i].first].fd_ref;
        if (events_desc[i].second == Fd::Flag::Read) {
          fd.on_read_event();
        } else {
          fd.on_write_event();
        }
      }
    }
  }

 private:
  struct FdInfo {
    Fd fd_ref;
    Fd::Flags flags;
  };
  std::vector<FdInfo> fds_;
};
}  // end of namespace detail
}  // end of namespace td
#endif  // TD_POLL_WINEVENT
