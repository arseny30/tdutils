#include "td/utils/port/detail/EventFdWindows.h"

char disable_linker_warning_about_empty_file_event_fd_windows_cpp TD_UNUSED;

#ifdef TD_EVENTFD_WINDOWS

namespace td {
namespace detail {

void EventFdWindows::init() {
  event_ = NativeFd(CreateEventW(nullptr, true, false, nullptr));
}

bool EventFdWindows::empty() {
  return !event_;
}

void EventFdWindows::close() {
  event_.close();
}

Status EventFdWindows::get_pending_error() {
  return Status::OK();
}

PollableFdInfo &EventFdWindows::get_poll_info() {
  UNREACHABLE();
}

void EventFdWindows::release() {
  SetEvent(event_.io_handle());
}

void EventFdWindows::acquire() {
  ResetEvent(event_.io_handle());
}

void EventFdWindows::wait(int timeout_ms) {
  WaitForSingleObject(event_.io_handle(), timeout_ms);
  ResetEvent(event_.io_handle());
}

}  // namespace detail
}  // namespace td

#endif
