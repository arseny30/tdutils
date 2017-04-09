#include "td/utils/port/config.h"
#ifdef TD_PORT_POSIX

#include "td/utils/port/SocketFd.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <algorithm>

namespace td {
/*** SocketFd ***/
SocketFd::operator FdRef() {
  return fd_;
}

Result<SocketFd> SocketFd::open(const IPAddress &address) {
  //  return Status::Error("Dummy error");
  SocketFd socket;
  TRY_STATUS(socket.init(address));
  return std::move(socket);
}

Result<SocketFd> SocketFd::from_native_fd(int fd) {
  SocketFd socket;
  TRY_STATUS(socket.init_socket(fd));
  socket.fd_ = Fd(fd, Fd::Mode::Own);
  return std::move(socket);
}

Status SocketFd::init_socket(int fd) {
  int err = fcntl(fd, F_SETFL, O_NONBLOCK);
  if (err == -1) {
    // TODO: can be interrupted by signal oO
    auto fcntl_errno = errno;
    auto error = Status::PosixError(fcntl_errno, "Failed to make socket nonblocking");
    ::close(fd);
    return error;
  }

  int flags = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &flags, sizeof(flags));
  setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &flags, sizeof(flags));
  setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flags, sizeof(flags));

  // TODO: SO_REUSEADDR, SO_KEEPALIVE, TCP_NODELAY, SO_SNDBUF, SO_RCVBUF,
  //      TCP_QUICKACK, SO_LINGER
  return Status::OK();
}

Status SocketFd::init(const IPAddress &address) {
  int fd = socket(address.get_address_family(), SOCK_STREAM, 0);
  if (fd == -1) {
    auto socket_errno = errno;
    return Status::PosixError(socket_errno, "Failed to create a socket");
  }

  TRY_STATUS(init_socket(fd));

  int err = connect(fd, address.get_sockaddr(), static_cast<socklen_t>(address.get_sockaddr_len()));
  if (err == -1) {
    auto connect_errno = errno;
    if (connect_errno != EINPROGRESS) {
      auto error = Status::PosixError(connect_errno, PSLICE() << "Failed to connect to " << address);
      ::close(fd);
      return error;
    }
  }
  fd_ = Fd(fd, Fd::Mode::Own);
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
  if (!(fd_.get_flags() & Fd::Error)) {
    return Status::OK();
  }
  fd_.clear_flags(Fd::Error);
  int error = 0;
  socklen_t errlen = sizeof(error);
  if (getsockopt(fd_.get_native_fd(), SOL_SOCKET, SO_ERROR, (void *)&error, &errlen) == 0) {
    if (error == 0) {
      return Status::OK();
    }
    return Status::PosixError(error, PSLICE() << "Error on socket [fd_ = " << fd_.get_native_fd() << "]");
  }
  auto getsockopt_errno = errno;
  LOG(INFO) << "can't load errno = " << getsockopt_errno;
  return Status::PosixError(getsockopt_errno,
                            PSLICE() << "Can't load error on socket [fd_ = " << fd_.get_native_fd() << "]");
}

Result<size_t> SocketFd::write(Slice slice) {
  return fd_.write(slice);
}

Result<size_t> SocketFd::read(MutableSlice slice) {
  return fd_.read(slice);
}

}  // namespace td
#endif  // TD_PORT_POSIX
