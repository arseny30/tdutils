#pragma once

#include "td/utils/port/config.h"

#include "td/utils/port/IPAddress.h"
#include "td/utils/port/Fd.h"
#include "td/utils/Slice.h"
#include "td/utils/Status.h"

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
  Fd fd_;

  friend class ServerSocketFd;

  Status init(const IPAddress &address) WARN_UNUSED_RESULT;

#if TD_PORT_POSIX
  static Result<SocketFd> from_native_fd(int fd);
#endif
#if TD_PORT_WINDOWS
  explicit SocketFd(Fd fd) : fd_(std::move(fd)) {
  }
#endif
};

}  // namespace td
