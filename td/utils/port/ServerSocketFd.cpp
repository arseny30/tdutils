#include "td/utils/port/config.h"
#ifdef TD_PORT_POSIX

#include "td/utils/port/ServerSocketFd.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>

namespace td {
ServerSocketFd::operator FdRef() {
  return fd_;
}

Result<ServerSocketFd> ServerSocketFd::open(int32 port) {
  ServerSocketFd socket;
  TRY_STATUS(socket.init(port));
  return std::move(socket);
}

const Fd &ServerSocketFd::get_fd() const {
  return fd_;
}

int32 ServerSocketFd::get_flags() const {
  return fd_.get_flags();
}

Status ServerSocketFd::get_pending_error() {
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
}

Result<SocketFd> ServerSocketFd::accept() {
  struct sockaddr_storage addr;
  socklen_t addr_len = sizeof(addr);
  int native_fd = fd_.get_native_fd();
  int r_fd;
  while (true) {
    r_fd = ::accept(native_fd, reinterpret_cast<struct sockaddr *>(&addr), &addr_len);
    if (r_fd != -1) {
      break;
    }
    auto accept_errno = errno;
    if (accept_errno == EINTR) {
      continue;
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
        break;
      default:
        LOG(ERROR) << error;
      // no break
      case EMFILE:
      case ENFILE:
      case ECONNABORTED:  //???
        fd_.clear_flags(Fd::Read);
        fd_.update_flags(Fd::Close);
        return std::move(error);
    }
    break;
  }
  TRY_RESULT(socket, SocketFd::from_native_fd(r_fd));
  return std::move(socket);
}

void ServerSocketFd::close() {
  fd_.close();
}

bool ServerSocketFd::empty() const {
  return fd_.empty();
}

Status ServerSocketFd::init(int32 port) {
  IPAddress address;
  TRY_STATUS(address.init_ipv4_port("0.0.0.0", port));
  int fd = socket(address.get_address_family(), SOCK_STREAM, 0);
  if (fd == -1) {
    auto socket_errno = errno;
    return Status::PosixError(socket_errno, "Failed to create a socket");
  }

  TRY_STATUS(init_socket(fd));

  int e_bind = bind(fd, address.get_sockaddr(), static_cast<socklen_t>(address.get_sockaddr_len()));
  if (e_bind == -1) {
    auto bind_errno = errno;
    auto error = Status::PosixError(bind_errno, "Failed to bind socket");
    ::close(fd);
    return error;
  }

  // TODO: magic constant
  int e_listen = listen(fd, 8192);
  if (e_listen == -1) {
    auto listen_errno = errno;
    auto error = Status::PosixError(listen_errno, "Failed to listen");
    ::close(fd);
    return error;
  }

  fd_ = Fd(fd, Fd::Mode::Own);
  return Status::OK();
}

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
#if !TD_ANDROID && !TD_WINDOWS
  setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &flags, sizeof(flags));
#endif
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &flags, sizeof(flags));
  setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &flags, sizeof(flags));
  setsockopt(fd, SOL_SOCKET, SO_LINGER, &ling, sizeof(ling));
  setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flags, sizeof(flags));

  return Status::OK();
}

}  // namespace td
#endif  // TD_PORT_POSIX
