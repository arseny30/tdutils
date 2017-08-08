#include "td/utils/port/config.h"

char disable_linker_warning_about_empty_file_event_fd_windows_cpp TD_UNUSED;

#ifdef TD_EVENTFD_WINDOWS

#include "td/utils/port/detail/EventFdWindows.h"

namespace td {
namespace detail {

void EventFdWindows::init() {
  fd_ = Fd(Fd::Type::EventFd, Fd::Mode::Owner, INVALID_HANDLE_VALUE);
}

bool EventFdWindows::empty() {
  return fd_.empty();
}

void EventFdWindows::close() {
  fd_.close();
}

Status EventFdWindows::get_pending_error() {
  return Status::OK();
}

const Fd &EventFdWindows::get_fd() const {
  return fd_;
}

Fd &EventFdWindows::get_fd() {
  return fd_;
}

void EventFdWindows::release() {
  fd_.release();
}

void EventFdWindows::acquire() {
  fd_.acquire();
}

}  // namespace detail
}  // namespace td

#endif  // TD_EVENTFD_WINDOWS
