#pragma once

#include "td/utils/port/config.h"

#ifdef TD_PORT_POSIX

#include "td/utils/port/IPAddress.h"
#include "td/utils/port/Fd.h"

#include "td/utils/misc.h"
#include "td/utils/Slice.h"
#include "td/utils/Status.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/socket.h>

namespace td {
class SocketFd {
 public:
  SocketFd() = default;
  SocketFd(const SocketFd &) = delete;
  SocketFd &operator=(const SocketFd &) = delete;
  SocketFd(SocketFd &&) = default;
  SocketFd &operator=(SocketFd &&) = default;

  operator FdRef();

  static Result<SocketFd> open(const IPAddress &address) WARN_UNUSED_RESULT;

  const Fd &get_fd() const;
  Fd &get_fd();
  bool can_read() const;
  bool can_write() const;
  bool can_close() const;
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

#include "td/utils/misc.h"
#include "td/utils/port/Fd.h"
#include "td/utils/Status.h"

namespace td {
class SocketFd : public Fd {
 public:
  static Result<SocketFd> open(const IPAddress &address) {
    auto fd = socket(address.get_address_family(), SOCK_STREAM, 0);
    if (fd == INVALID_SOCKET) {
      return Status::WsaError("Failed to create a socket");
    }

    Status res = init_socket(fd);
    if (!res.is_ok()) {
      return std::move(res);
    }

    auto bind_addr = address.get_any_addr();
    auto e_bind = bind(fd, bind_addr.get_sockaddr(), narrow_cast<int>(bind_addr.get_sockaddr_len()));
    if (e_bind != 0) {
      return Status::WsaError("Failed to bind a socket");
    }

    auto sock = SocketFd(Fd::Type::SocketFd, Fd::Mode::Owner, fd, address.get_address_family());
    sock.connect(address);
    return std::move(sock);
  }

  explicit SocketFd(Fd fd) : Fd(std::move(fd)) {
  }

  SocketFd() = default;
  SocketFd(const SocketFd &) = delete;
  SocketFd &operator=(const SocketFd &) = delete;
  SocketFd(SocketFd &&) = default;
  SocketFd &operator=(SocketFd &&) = default;

  using Fd::connect;
  using Fd::write;
  using Fd::read;
  using Fd::close;
  using Fd::empty;
  using Fd::get_flags;
  using Fd::has_pending_error;
  using Fd::get_pending_error;

 private:
  static Status init_socket(SOCKET fd) WARN_UNUSED_RESULT {
    u_long iMode = 1;
    int err = ioctlsocket(fd, FIONBIO, &iMode);
    if (err != 0) {
      ::closesocket(fd);
      return Status::WsaError("Failed to make socket nonblocking");
    }

    int flags = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char *>(&flags), sizeof(flags));
    setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast<char *>(&flags), sizeof(flags));

    // TODO: SO_REUSEADDR, SO_KEEPALIVE, TCP_NODELAY, SO_SNDBUF, SO_RCVBUF,
    //      TCP_QUICKACK, SO_LINGER
    return Status::OK();
  }
  using Fd::Fd;
};
}  // namespace td
#endif  // TD_PORT_WINDOWS
