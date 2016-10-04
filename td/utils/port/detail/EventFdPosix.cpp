#include "td/utils/port/config.h"

#ifdef TD_EVENTFD_POSIX

#include <sys/eventfd.h>

#include "td/utils/port/detail/EventFdPosix.h"

namespace td {
namespace detail {
EventFdPosix::operator FdRef() {
  return get_fd();
}
void EventFdPosix::init() {
  int fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
  auto eventfd_errno = errno;
  LOG_IF(FATAL, fd == -1) << Status::PosixError(eventfd_errno, "eventfd call failed");

  fd_ = Fd(fd, Fd::Mode::Own);
}

bool EventFdPosix::empty() {
  return fd_.empty();
}

void EventFdPosix::close() {
  fd_.close();
}

Status EventFdPosix::get_pending_error() {
  return Status::OK();
}

const Fd &EventFdPosix::get_fd() const {
  return fd_;
}

Fd &EventFdPosix::get_fd() {
  return fd_;
}

void EventFdPosix::release() {
  uint64 value = 1;
  // NB: write_unsafe is used, because release will be call from multible threads.
  auto result = fd_.write_unsafe(Slice(&value, sizeof(value)));
  if (result.is_error()) {
    LOG(FATAL) << "EventFdPosix write failed: " << result.error();
  }
  size_t size = result.ok();
  if (size != sizeof(value)) {
    LOG(FATAL) << "EventFdPosix write returned " << value << " instead of " << sizeof(value);
  }
}

void EventFdPosix::acquire() {
  uint64 res;
  auto result = fd_.read(MutableSlice(&res, sizeof(res)));
  if (result.is_error()) {
    if (result.error().code() == EAGAIN || result.error().code() == EWOULDBLOCK) {
    } else {
      LOG(FATAL) << "EventFdPosix read failed: " << result.error();
    }
  }
  fd_.clear_flags(Fd::Read);
}
}  // end of namespace detail
}  // end of namespace td

#endif  // TD_EVENTFD_POSIX
