#pragma once

#include "td/utils/port/config.h"

#include "td/utils/port/Fd.h"
#include "td/utils/port/SocketFd.h"

#include "td/utils/Slice.h"
#include "td/utils/Status.h"

#ifdef TD_PORT_POSIX

namespace td {

class ServerSocketFd {
 public:
  ServerSocketFd() = default;
  ServerSocketFd(const ServerSocketFd &) = delete;
  ServerSocketFd &operator=(const ServerSocketFd &) = delete;
  ServerSocketFd(ServerSocketFd &&) = default;
  ServerSocketFd &operator=(ServerSocketFd &&) = default;

  static Result<ServerSocketFd> open(int32 port, CSlice addr = CSlice("0.0.0.0")) WARN_UNUSED_RESULT;

  const Fd &get_fd() const;
  Fd &get_fd();
  int32 get_flags() const;
  Status get_pending_error() WARN_UNUSED_RESULT;

  Result<SocketFd> accept() WARN_UNUSED_RESULT;

  void close();
  bool empty() const;

 private:
  Fd fd_;
  Status init(int32 port, CSlice addr) WARN_UNUSED_RESULT;
  Status init_socket(int fd) WARN_UNUSED_RESULT;
};

}  // namespace td
#endif  // TD_PORT_POSIX

#ifdef TD_PORT_WINDOWS

namespace td {

class ServerSocketFd : public Fd {
 public:
  ServerSocketFd() = default;
  ServerSocketFd(const ServerSocketFd &) = delete;
  ServerSocketFd &operator=(const ServerSocketFd &) = delete;
  ServerSocketFd(ServerSocketFd &&) = default;
  ServerSocketFd &operator=(ServerSocketFd &&) = default;

  static Result<ServerSocketFd> open(int32 port, CSlice addr = CSlice("0.0.0.0")) WARN_UNUSED_RESULT;

  using Fd::get_fd;
  using Fd::get_flags;
  using Fd::get_pending_error;

  Result<SocketFd> accept() WARN_UNUSED_RESULT;

  using Fd::close;
  using Fd::empty;

 private:
  static Status init_socket(SOCKET fd) WARN_UNUSED_RESULT;

  using Fd::Fd;
};

}  // namespace td
#endif  // TD_PORT_WINDOWS
