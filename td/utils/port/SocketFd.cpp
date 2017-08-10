#include "td/utils/port/config.h"

#include "td/utils/port/SocketFd.h"

#ifdef TD_PORT_WINDOWS
#include "td/utils/misc.h"
#endif

#ifdef TD_PORT_POSIX
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace td {

Result<SocketFd> SocketFd::open(const IPAddress &address) {
  SocketFd socket;
  TRY_STATUS(socket.init(address));
  return std::move(socket);
}

#ifdef TD_PORT_POSIX
Result<SocketFd> SocketFd::from_native_fd(int fd) {
  auto fd_quard = ScopeExit() + [fd]() { ::close(fd); };

  int err = fcntl(fd, F_SETFL, O_NONBLOCK);
  if (err == -1) {
    auto fcntl_errno = errno;
    return Status::PosixError(fcntl_errno, "Failed to make socket nonblocking");
  }

  // TODO remove copypaste
  int flags = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&flags), sizeof(flags));
  setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast<const char *>(&flags), sizeof(flags));
  setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char *>(&flags), sizeof(flags));
  // TODO: SO_REUSEADDR, SO_KEEPALIVE, TCP_NODELAY, SO_SNDBUF, SO_RCVBUF, TCP_QUICKACK, SO_LINGER

  fd_guard.dismiss();

  SocketFd socket;
  socket.fd_ = Fd(fd, Fd::Mode::Own);
  return std::move(socket);
}
#endif

Status SocketFd::init(const IPAddress &address) {
  auto fd = socket(address.get_address_family(), SOCK_STREAM, 0);
#ifdef TD_PORT_POSIX
  if (fd == -1) {
    auto socket_errno = errno;
    return Status::PosixError(socket_errno, "Failed to create a socket");
  }
#endif
#ifdef TD_PORT_WINDOWS
  if (fd == INVALID_SOCKET) {
    return Status::WsaError("Failed to create a socket");
  }
#endif
  auto fd_quard = ScopeExit() + [fd]() {
#ifdef TD_PORT_POSIX
    ::close(fd);
#endif
#ifdef TD_PORT_WINDOWS
    ::closesocket(fd);
#endif
  };

#ifdef TD_PORT_POSIX
  int err = fcntl(fd, F_SETFL, O_NONBLOCK);
  if (err == -1) {
    auto fcntl_errno = errno;
    return Status::PosixError(fcntl_errno, "Failed to make socket nonblocking");
  }
#endif
#ifdef TD_PORT_WINDOWS
  u_long iMode = 1;
  int err = ioctlsocket(fd, FIONBIO, &iMode);
  if (err != 0) {
    return Status::WsaError("Failed to make socket nonblocking");
  }
#endif

#ifdef TD_PORT_POSIX
  int flags = 1;
#endif
#ifdef TD_PORT_WINDOWS
  BOOL flags = TRUE;
#endif
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&flags), sizeof(flags));
  setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast<const char *>(&flags), sizeof(flags));
  setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char *>(&flags), sizeof(flags));
// TODO: SO_REUSEADDR, SO_KEEPALIVE, TCP_NODELAY, SO_SNDBUF, SO_RCVBUF, TCP_QUICKACK, SO_LINGER

#ifdef TD_PORT_POSIX
  int e_connect = connect(fd, address.get_sockaddr(), static_cast<socklen_t>(address.get_sockaddr_len()));
  if (e_connect == -1) {
    auto connect_errno = errno;
    if (connect_errno != EINPROGRESS) {
      return Status::PosixError(connect_errno, PSLICE() << "Failed to connect to " << address);
    }
  }
  fd_ = Fd(fd, Fd::Mode::Own);
#endif
#ifdef TD_PORT_WINDOWS
  auto bind_addr = address.get_any_addr();
  auto e_bind = bind(fd, bind_addr.get_sockaddr(), narrow_cast<int>(bind_addr.get_sockaddr_len()));
  if (e_bind != 0) {
    return Status::WsaError("Failed to bind a socket");
  }

  fd_ = Fd(Fd::Type::SocketFd, Fd::Mode::Owner, fd, address.get_address_family());
  fd_.connect(address);
#endif

  fd_quard.dismiss();
  return Status::OK();
}

const Fd &SocketFd::get_fd() const {
  return fd_;
}

Fd &SocketFd::get_fd() {
  return fd_;
}

void SocketFd::close() {
  fd_.close();
}

bool SocketFd::empty() const {
  return fd_.empty();
}

int32 SocketFd::get_flags() const {
  return fd_.get_flags();
}

Status SocketFd::get_pending_error() {
#ifdef TD_PORT_POSIX
  if (!(fd_.get_flags() & Fd::Error)) {
    return Status::OK();
  }
  fd_.clear_flags(Fd::Error);
  int error = 0;
  socklen_t errlen = sizeof(error);
  if (getsockopt(fd_.get_native_fd(), SOL_SOCKET, SO_ERROR, static_cast<void *>(&error), &errlen) == 0) {
    if (error == 0) {
      return Status::OK();
    }
    return Status::PosixError(error, PSLICE() << "Error on socket [fd_ = " << fd_.get_native_fd() << "]");
  }
  auto getsockopt_errno = errno;
  LOG(INFO) << "Can't load errno = " << getsockopt_errno;
  return Status::PosixError(getsockopt_errno,
                            PSLICE() << "Can't load error on socket [fd_ = " << fd_.get_native_fd() << "]");
#endif
#ifdef TD_PORT_WINDOWS
  return fd_.get_pending_error();
#endif
}

Result<size_t> SocketFd::write(Slice slice) {
  return fd_.write(slice);
}

Result<size_t> SocketFd::read(MutableSlice slice) {
  return fd_.read(slice);
}

}  // namespace td
