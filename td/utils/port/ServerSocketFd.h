#pragma once

#include "td/utils/port/config.h"

#ifdef TD_PORT_POSIX

#include "td/utils/port/IPAddress.h"
#include "td/utils/port/Fd.h"
#include "td/utils/port/SocketFd.h"

#include "td/utils/Slice.h"
#include "td/utils/Status.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/socket.h>

namespace td {
class ServerSocketFd {
 public:
  ServerSocketFd() = default;
  ServerSocketFd(const ServerSocketFd &) = delete;
  ServerSocketFd &operator=(const ServerSocketFd &) = delete;
  ServerSocketFd(ServerSocketFd &&) = default;
  ServerSocketFd &operator=(ServerSocketFd &&) = default;

  operator FdRef();

  static Result<ServerSocketFd> open(int32 port, CSlice addr = "0.0.0.0") WARN_UNUSED_RESULT;

  const Fd &get_fd() const;
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
#include "td/utils/port/Fd.h"
#include "td/utils/port/SocketFd.h"

namespace td {
class ServerSocketFd : public Fd {
 public:
  static Result<ServerSocketFd> open(int32 port, CSlice addr = "0.0.0.0") {
    IPAddress address;
    auto status = address.init_ipv4_port(addr, port);
    if (status.is_error()) {
      return std::move(status);
    }
    auto fd = socket(address.get_address_family(), SOCK_STREAM, 0);
    if (fd == INVALID_SOCKET) {
      return Status::WsaError("Failed to create a socket");
    }

    status = init_socket(fd);
    if (!status.is_ok()) {
      return std::move(status);
    }

    int e_bind = bind(fd, address.get_sockaddr(), static_cast<socklen_t>(address.get_sockaddr_len()));
    if (e_bind != 0) {
      ::closesocket(fd);
      return Status::WsaError("Failed to bind socket");
    }

    // TODO: magic constant
    int e_listen = listen(fd, 8192);
    if (e_listen != 0) {
      ::closesocket(fd);
      return Status::WsaError("Failed to listen");
    }

    return ServerSocketFd(Fd::Type::ServerSocketFd, Fd::Mode::Owner, fd, address.get_address_family());
  }

  ServerSocketFd() = default;
  ServerSocketFd(const ServerSocketFd &) = delete;
  ServerSocketFd &operator=(const ServerSocketFd &) = delete;
  ServerSocketFd(ServerSocketFd &&) = default;
  ServerSocketFd &operator=(ServerSocketFd &&) = default;

  Result<SocketFd> accept() WARN_UNUSED_RESULT {
    auto r_socket = Fd::accept();
    if (r_socket.is_error()) {
      return r_socket.move_as_error();
    }
    return SocketFd(r_socket.move_as_ok());
  }

  using Fd::empty;
  using Fd::close;
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
    struct linger ling = {0, 0};
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char *>(&flags), sizeof(flags));
    setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast<char *>(&flags), sizeof(flags));
    setsockopt(fd, SOL_SOCKET, SO_LINGER, reinterpret_cast<char *>(&ling), sizeof(ling));
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char *>(&flags), sizeof(flags));

    return Status::OK();
  }

  using Fd::Fd;
};

}  // namespace td
#endif  // TD_PORT_WINDOWS
