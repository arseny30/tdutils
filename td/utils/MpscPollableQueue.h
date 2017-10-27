#pragma once

#include "td/utils/misc.h"
#include "td/utils/port/EventFd.h"

#if !TD_EVENTFD_UNSUPPORTED

#if !TD_WINDOWS
#include <poll.h>  // for pollfd, poll, POLLIN
#include <sched.h>
#endif

#include <utility>

#include <td/utils/SpinLock.h>

namespace td {
// interface like in PollableQueue
template <class ValueT>
class MpscPollableQueue {
 public:
  int reader_wait_nonblock() {
    auto ready = reader_vector_.size() - reader_pos_;
    if (ready != 0) {
      return narrow_cast<int>(ready);
    }

    auto guard = lock_.lock();
    if (writer_vector_.empty()) {
      event_fd_.acquire();
      wait_event_fd_ = true;
      return 0;
    } else {
      reader_vector_.clear();
      reader_pos_ = 0;
      std::swap(writer_vector_, reader_vector_);
      return narrow_cast<int>(reader_vector_.size());
    }
  }
  ValueT reader_get_unsafe() {
    return std::move(reader_vector_[reader_pos_++]);
  }
  void reader_flush() {
    //nop
  }
  void writer_put(ValueT value) {
    auto guard = lock_.lock();
    writer_vector_.push_back(std::move(value));
    if (wait_event_fd_) {
      wait_event_fd_ = false;
      event_fd_.release();
    }
  }
  EventFd& reader_get_event_fd() {
    return event_fd_;
  }
  void writer_flush() {
    //nop
  }

  void init() {
    event_fd_.init();
  }
  void destroy() {
    if (!event_fd_.empty()) {
      event_fd_.close();
      wait_event_fd_ = false;
      writer_vector_.clear();
      reader_vector_.clear();
      reader_pos_ = 0;
    }
  }

// Just example of usage
#if !TD_WINDOWS
  int reader_wait() {
    int res;

    while ((res = reader_wait_nonblock()) == 0) {
      // TODO: reader_flush?
      pollfd fd;
      fd.fd = reader_get_event_fd().get_fd().get_native_fd();
      fd.events = POLLIN;
      poll(&fd, 1, -1);
    }
    return res;
  }
#endif

 private:
  SpinLock lock_;
  bool wait_event_fd_{false};
  EventFd event_fd_;
  std::vector<ValueT> writer_vector_;
  std::vector<ValueT> reader_vector_;
  size_t reader_pos_{0};
};

}  // namespace td

#endif
