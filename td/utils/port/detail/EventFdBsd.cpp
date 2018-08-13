#include "td/utils/port/detail/EventFdBsd.h"

char disable_linker_warning_about_empty_file_event_fd_bsd_cpp TD_UNUSED;

#ifdef TD_EVENTFD_BSD

#include "td/utils/logging.h"
#include "td/utils/Slice.h"
#include "td/utils/port/SocketFd.h"

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>

namespace td {
namespace detail {

// TODO: it is extremely non optimal on Darwin. kqueue events should be used instead
void EventFdBsd::init() {
  int fds[2];
  int err = socketpair(AF_UNIX, SOCK_STREAM, 0, fds);
  auto socketpair_errno = errno;
#if TD_CYGWIN
  // it looks like CYGWIN bug
  int max_retries = 1000000;
  while (err == -1 && socketpair_errno == EADDRINUSE && max_retries-- > 0) {
    err = socketpair(AF_UNIX, SOCK_STREAM, 0, fds);
    socketpair_errno = errno;
  }
// LOG_IF(ERROR, max_retries < 1000000) << max_retries;
#endif
  LOG_IF(FATAL, err == -1) << Status::PosixError(socketpair_errno, "socketpair failed");

  auto fd_a = NativeFd(fds[0]);
  auto fd_b = NativeFd(fds[1]);
  detail::set_native_socket_is_blocking(fd_a, false).ensure();
  detail::set_native_socket_is_blocking(fd_b, false).ensure();

  in_ = SocketFd::from_native_fd(std::move(fd_a)).move_as_ok();
  out_ = SocketFd::from_native_fd(std::move(fd_b)).move_as_ok();
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

PollableFdInfo &EventFdBsd::get_poll_info() {
  return out_.get_poll_info();
}

void EventFdBsd::release() {
  int value = 1;
  auto result = in_.write(Slice(reinterpret_cast<const char *>(&value), sizeof(value)));
  if (result.is_error()) {
    LOG(FATAL) << "EventFdBsd write failed: " << result.error();
  }
  size_t size = result.ok();
  if (size != sizeof(value)) {
    LOG(FATAL) << "EventFdBsd write returned " << value << " instead of " << sizeof(value);
  }
}

void EventFdBsd::acquire() {
  out_.get_poll_info().add_flags(PollFlags::Read());
  while (can_read(out_)) {
    uint8 value[1024];
    auto result = out_.read(MutableSlice(value, sizeof(value)));
    if (result.is_error()) {
      LOG(FATAL) << "EventFdBsd read failed:" << result.error();
    }
  }
}

}  // namespace detail
}  // namespace td

#endif
