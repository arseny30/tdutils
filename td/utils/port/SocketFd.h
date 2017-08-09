#pragma once

#include "td/utils/port/config.h"

#include "td/utils/port/IPAddress.h"
#include "td/utils/port/Fd.h"
#include "td/utils/Slice.h"
#include "td/utils/Status.h"

#ifdef TD_PORT_POSIX

namespace td {

class SocketFd {
 public:
  SocketFd() = default;
  SocketFd(const SocketFd &) = delete;
  SocketFd &operator=(const SocketFd &) = delete;
  SocketFd(SocketFd &&) = default;
  SocketFd &operator=(SocketFd &&) = default;

  static Result<SocketFd> open(const IPAddress &address) WARN_UNUSED_RESULT;

  const Fd &get_fd() const;
  Fd &get_fd();

  int32 get_flags() const;

  Status get_pending_error() WARN_UNUSED_RESULT;

  Result<size_t> write(Slice slice) WARN_UNUSED_RESULT;
  Result<size_t> read(MutableSlice slice) WARN_UNUSED_RESULT;

  void close();
  bool empty() const;

 private:
  friend class ServerSocketFd;
  static Result<SocketFd> from_native_fd(int fd);
  Fd fd_;
  Status init(const IPAddress &address) WARN_UNUSED_RESULT;
  Status init_socket(int fd) WARN_UNUSED_RESULT;
};

}  // namespace td
#endif  // TD_PORT_POSIX

#ifdef TD_PORT_WINDOWS

namespace td {
class SocketFd : public Fd {
 public:
  SocketFd() = default;
  SocketFd(const SocketFd &) = delete;
  SocketFd &operator=(const SocketFd &) = delete;
  SocketFd(SocketFd &&) = default;
  SocketFd &operator=(SocketFd &&) = default;

  static Result<SocketFd> open(const IPAddress &address) WARN_UNUSED_RESULT;

  explicit SocketFd(Fd fd) : Fd(std::move(fd)) {
  }

  using Fd::get_fd;

  using Fd::get_flags;

  using Fd::get_pending_error;

  using Fd::write;
  using Fd::read;
  using Fd::close;
  using Fd::empty;

 private:
  static Status init_socket(SOCKET fd) WARN_UNUSED_RESULT;
  using Fd::Fd;
};
}  // namespace td
#endif  // TD_PORT_WINDOWS
