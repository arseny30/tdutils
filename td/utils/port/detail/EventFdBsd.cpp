#include "td/utils/port/config.h"

#ifdef TD_EVENTFD_BSD

#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>

#include "td/utils/port/detail/EventFdBsd.h"

namespace td {
namespace detail {
EventFdBsd::operator FdRef() {
  return get_fd();
}
// TODO: it is extreemly non optimal. kqueue events should be used for mac
void EventFdBsd::init() {
  int fds[2];
  int err = socketpair(AF_UNIX, SOCK_STREAM, 0, fds);
  auto socketpair_errno = errno;
  LOG_IF(FATAL, err == -1) << Status::PosixError(socketpair_errno, "socketpair failed");

  err = fcntl(fds[0], F_SETFL, O_NONBLOCK);
  auto fcntl_errno = errno;
  LOG_IF(FATAL, err == -1) << Status::PosixError(fcntl_errno, "fcntl 0 failed");

  err = fcntl(fds[1], F_SETFL, O_NONBLOCK);
  fcntl_errno = errno;
  LOG_IF(FATAL, err == -1) << Status::PosixError(fcntl_errno, "fcntl 1 failed");

  in_ = Fd(fds[0], Fd::Mode::Own);
  out_ = Fd(fds[1], Fd::Mode::Own);
}

bool EventFdBsd::empty() {
  return in_.empty();
}

void EventFdBsd::close() {
  in_.close();
  out_.close();
}

Status EventFdBsd::get_pending_error() {
  return Status::OK();
}

const Fd &EventFdBsd::get_fd() const {
  return out_;
}

Fd &EventFdBsd::get_fd() {
  return out_;
}

void EventFdBsd::release() {
  int value = 1;
  auto result = in_.write(Slice(&value, sizeof(value)));
  if (result.is_error()) {
    LOG(FATAL) << "EventFdBsd write failed: " << result.error();
  }
  size_t size = result.ok();
  if (size != sizeof(value)) {
    LOG(FATAL) << "EventFdBsd write returned " << value << " instead of " << sizeof(value);
  }
}

void EventFdBsd::acquire() {
  out_.update_flags(Fd::Read);
  while (can_read(out_)) {
    uint8 value[1024];
    auto result = out_.read(MutableSlice(value, sizeof(value)));
    if (result.is_error()) {
      LOG(FATAL) << "EventFdBsd read failed:" << result.error();
    }
  }
}
}  // end of namespace detail
}  // end of namespace td

#endif  // TD_EVENTFD_BSD
