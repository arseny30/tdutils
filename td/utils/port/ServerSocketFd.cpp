#include "td/utils/port/config.h"

#include "td/utils/port/IPAddress.h"
#include "td/utils/port/ServerSocketFd.h"

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

Result<ServerSocketFd> ServerSocketFd::open(int32 port, CSlice addr) {
  ServerSocketFd result_socket;
  TRY_STATUS(result_socket.init(port, addr));
  return std::move(result_socket);
}

const Fd &ServerSocketFd::get_fd() const {
  return fd_;
}

Fd &ServerSocketFd::get_fd() {
  return fd_;
}

int32 ServerSocketFd::get_flags() const {
  return fd_.get_flags();
}

Status ServerSocketFd::get_pending_error() {
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

Result<SocketFd> ServerSocketFd::accept() {
#ifdef TD_PORT_POSIX
  struct sockaddr_storage addr;
  socklen_t addr_len = sizeof(addr);
  int native_fd = fd_.get_native_fd();
  int r_fd = skip_eintr([&] { return ::accept(native_fd, reinterpret_cast<struct sockaddr *>(&addr), &addr_len); });
  auto accept_errno = errno;
  if (r_fd >= 0) {
    return SocketFd::from_native_fd(r_fd);
  }

  if (accept_errno == EAGAIN) {
    fd_.clear_flags(Fd::Read);
    return Status::PosixError<EAGAIN>();
  }
#if EAGAIN != EWOULDBLOCK
  if (accept_errno == EWOULDBLOCK) {
    fd_.clear_flags(Fd::Read);
    return Status::PosixError<EWOULDBLOCK>();
  }
#endif

  auto error = Status::PosixError(accept_errno, PSLICE() << "Accept from [fd = " << native_fd << "] has failed");
  switch (accept_errno) {
    case EBADF:
    case EFAULT:
    case EINVAL:
    case ENOTSOCK:
    case EOPNOTSUPP:
      LOG(FATAL) << error;
      UNREACHABLE();
      break;
    default:
      LOG(ERROR) << error;
    // fallthrough
    case EMFILE:
    case ENFILE:
    case ECONNABORTED:  //???
      fd_.clear_flags(Fd::Read);
      fd_.update_flags(Fd::Close);
      return std::move(error);
  }
#endif
#ifdef TD_PORT_WINDOWS
  auto r_socket = fd_.accept();
  if (r_socket.is_error()) {
    return r_socket.move_as_error();
  }
  return SocketFd(r_socket.move_as_ok());
#endif
}

void ServerSocketFd::close() {
  fd_.close();
}

bool ServerSocketFd::empty() const {
  return fd_.empty();
}

Status ServerSocketFd::init(int32 port, CSlice addr) {
  IPAddress address;
  TRY_STATUS(address.init_ipv4_port(addr, port));
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

  TRY_STATUS(init_socket(fd));

  int e_bind = bind(fd, address.get_sockaddr(), static_cast<socklen_t>(address.get_sockaddr_len()));
  if (e_bind != 0) {
#ifdef TD_PORT_POSIX
    auto bind_errno = errno;
    auto error = Status::PosixError(bind_errno, "Failed to bind socket");
    ::close(fd);
#endif
#ifdef TD_PORT_WINDOWS
    auto error = Status::WsaError("Failed to bind socket");
    ::closesocket(fd);
#endif
    return error;
  }

  // TODO: magic constant
  int e_listen = listen(fd, 8192);
  if (e_listen != 0) {
#ifdef TD_PORT_POSIX
    auto listen_errno = errno;
    auto error = Status::PosixError(listen_errno, "Failed to listen");
    ::close(fd);
#endif
#ifdef TD_PORT_WINDOWS
    auto error = Status::WsaError("Failed to listen");
    ::closesocket(fd);
#endif
    return error;
  }

#ifdef TD_PORT_POSIX
  fd_ = Fd(fd, Fd::Mode::Own);
#endif
#ifdef TD_PORT_WINDOWS
  fd_ = Fd(Fd::Type::ServerSocketFd, Fd::Mode::Owner, fd, address.get_address_family());
#endif
  return Status::OK();
}

#ifdef TD_PORT_POSIX
Status ServerSocketFd::init_socket(int fd) {
  int err = fcntl(fd, F_SETFL, O_NONBLOCK);
  if (err == -1) {
    // TODO: can be interrupted by a signal oO
    auto fcntl_errno = errno;
    auto error = Status::PosixError(fcntl_errno, "Failed to make socket nonblocking");
    ::close(fd);
    return error;
  }

  int flags = 1;
  struct linger ling = {0, 0};
#if !TD_ANDROID && !TD_CYGWIN
  setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &flags, sizeof(flags));
#endif
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &flags, sizeof(flags));
  setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &flags, sizeof(flags));
  setsockopt(fd, SOL_SOCKET, SO_LINGER, &ling, sizeof(ling));
  setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flags, sizeof(flags));

  return Status::OK();
}
#endif

#ifdef TD_PORT_WINDOWS
Status ServerSocketFd::init_socket(SOCKET fd) {
  u_long iMode = 1;
  int err = ioctlsocket(fd, FIONBIO, &iMode);
  if (err != 0) {
    auto error = Status::WsaError("Failed to make socket nonblocking");
    ::closesocket(fd);
    return error;
  }

  BOOL flags = TRUE;
  struct linger ling = {0, 0};
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&flags), sizeof(flags));
  setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast<const char *>(&flags), sizeof(flags));
  setsockopt(fd, SOL_SOCKET, SO_LINGER, reinterpret_cast<const char *>(&ling), sizeof(ling));
  setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char *>(&flags), sizeof(flags));

  return Status::OK();
}
#endif

}  // namespace td
