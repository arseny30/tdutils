#pragma once

#include "td/utils/port/config.h"

#include "td/utils/port/detail/NativeFd.h"
#include "td/utils/port/detail/PollableFd.h"
#include "td/utils/port/IPAddress.h"
#include "td/utils/Slice.h"
#include "td/utils/Status.h"

#include <memory>

namespace td {

namespace detail {
class SocketFdImpl;
class SocketFdImplDeleter {
 public:
  void operator()(SocketFdImpl *impl);
};
}  // namespace detail

class SocketFd {
 public:
  SocketFd();
  SocketFd(const SocketFd &) = delete;
  SocketFd &operator=(const SocketFd &) = delete;
  SocketFd(SocketFd &&);
  SocketFd &operator=(SocketFd &&);
  ~SocketFd();

  static Result<SocketFd> open(const IPAddress &address) TD_WARN_UNUSED_RESULT;

  PollableFdInfo &get_poll_info();
  const PollableFdInfo &get_poll_info() const;

  Status get_pending_error() TD_WARN_UNUSED_RESULT;

  Result<size_t> write(Slice slice) TD_WARN_UNUSED_RESULT;
  Result<size_t> read(MutableSlice slice) TD_WARN_UNUSED_RESULT;

  const NativeFd &get_native_fd() const;
  static Result<SocketFd> from_native_fd(NativeFd fd);

  void close();
  bool empty() const;

 private:
  std::unique_ptr<detail::SocketFdImpl, detail::SocketFdImplDeleter> impl_;
  explicit SocketFd(unique_ptr<detail::SocketFdImpl> impl);
};

namespace detail {
#if TD_PORT_POSIX
Status get_socket_pending_error(const NativeFd &fd);
#elif TD_PORT_WINDOWS
Status get_socket_pending_error(const NativeFd &fd, WSAOVERLAPPED *overlapped, Status iocp_error);
#endif
}  // namespace detail

}  // namespace td
