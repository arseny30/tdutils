#include "td/utils/port/detail/NativeFd.h"

#include "td/utils/format.h"
#include "td/utils/logging.h"
#include "td/utils/Status.h"

#if TD_PORT_POSIX
#include <fcntl.h>
#include <unistd.h>
#endif

#if TD_FD_DEBUG
#include <set>
#include <mutex>
#endif

namespace td {

#if TD_FD_DEBUG
class FdSet {
 public:
  void on_create_fd(NativeFd::Fd fd) {
    CHECK(fd >= 0);
    if (is_stdio(fd)) {
      return;
    }
    std::unique_lock<std::mutex> guard(mutex_);
    if (fds_.count(fd) > 1) {
      LOG(FATAL) << "Create duplicated fd: " << fd;
    }
    fds_.insert(fd);
  }

  void on_release_fd(NativeFd::Fd fd) {
    CHECK(fd >= 0);
    if (is_stdio(fd)) {
      return;
    }
    LOG(FATAL) << "Unexpected release of non stdio NativeFd: " << fd;
  }

  Status validate(NativeFd::Fd fd) {
    if (fd < 0) {
      return Status::Error(PSLICE() << "Invalid fd: " << fd);
    }
    if (is_stdio(fd)) {
      return Status::OK();
    }
    std::unique_lock<std::mutex> guard(mutex_);
    if (fds_.count(fd) != 1) {
      return Status::Error(PSLICE() << "Unknown fd: " << fd);
    }
    return Status::OK();
  }

  void on_close_fd(NativeFd::Fd fd) {
    CHECK(fd >= 0);
    if (is_stdio(fd)) {
      return;
    }
    std::unique_lock<std::mutex> guard(mutex_);
    if (fds_.count(fd) != 1) {
      LOG(FATAL) << "Close unknown fd: " << fd;
    }
    fds_.erase(fd);
  }

 private:
  std::mutex mutex_;
  std::set<NativeFd::Fd> fds_;

  bool is_stdio(NativeFd::Fd fd) {
    return fd >= 0 && fd <= 2;
  }
};

namespace {
FdSet &get_fd_set() {
  static FdSet res;
  return res;
}
}  // namespace

#endif
Status NativeFd::validate() const {
#if TD_FD_DEBUG
  return get_fd_set().validate(fd_.get());
#else
  return Status::OK();
#endif
}

NativeFd::NativeFd(Fd fd) : fd_(fd) {
  VLOG(fd) << *this << " create";
#if TD_FD_DEBUG
  get_fd_set().on_create_fd(fd);
#endif
}

NativeFd::NativeFd(Fd fd, bool nolog) : fd_(fd) {
#if TD_FD_DEBUG
  get_fd_set().on_create_fd(fd);
#endif
}

#if TD_PORT_WINDOWS
NativeFd::NativeFd(Socket socket) : fd_(reinterpret_cast<Fd>(socket)), is_socket_(true) {
  VLOG(fd) << *this << " create";
}
#endif
NativeFd &NativeFd::operator=(NativeFd &&from) {
  close();
  fd_ = std::move(from.fd_);
#if TD_PORT_WINDOWS
  is_socket_ = from.is_socket_;
#endif
  return *this;
}

NativeFd::~NativeFd() {
  close();
}

NativeFd::operator bool() const {
  return fd_.get() != empty_fd();
}

NativeFd::Fd NativeFd::empty_fd() {
#if TD_PORT_POSIX
  return -1;
#elif TD_PORT_WINDOWS
  return INVALID_HANDLE_VALUE;
#endif
}

NativeFd::Fd NativeFd::fd() const {
  return fd_.get();
}

NativeFd::Socket NativeFd::socket() const {
#if TD_PORT_POSIX
  return fd();
#elif TD_PORT_WINDOWS
  CHECK(is_socket_);
  return reinterpret_cast<Socket>(fd_.get());
#endif
}

Status NativeFd::set_is_blocking(bool is_blocking) const {
#if TD_PORT_POSIX
  auto old_flags = fcntl(fd(), F_GETFL);
  if (old_flags == -1) {
    return OS_SOCKET_ERROR("Failed to get socket flags");
  }
  auto new_flags = is_blocking ? old_flags & ~O_NONBLOCK : old_flags | O_NONBLOCK;
  if (new_flags != old_flags && fcntl(fd(), F_SETFL, new_flags) == -1) {
    return OS_SOCKET_ERROR("Failed to set socket flags");
  }

  return Status::OK();
#elif TD_PORT_WINDOWS
  return set_is_blocking_unsafe(is_blocking);
#endif
}

Status NativeFd::set_is_blocking_unsafe(bool is_blocking) const {
#if TD_PORT_POSIX
  if (fcntl(fd(), F_SETFL, is_blocking ? 0 : O_NONBLOCK) == -1) {
#elif TD_PORT_WINDOWS
  u_long mode = is_blocking;
  if (ioctlsocket(socket(), FIONBIO, &mode) != 0) {
#endif
    return OS_SOCKET_ERROR("Failed to change socket flags");
  }
  return Status::OK();
}

Status NativeFd::duplicate(const NativeFd &to) const {
#if TD_PORT_POSIX
  CHECK(*this);
  CHECK(to);
  if (dup2(fd(), to.fd()) == -1) {
    return OS_ERROR("Failed to duplicate file descriptor");
  }
  return Status::OK();
#elif TD_PORT_WINDOWS
  return Status::Error("Not supported");
#endif
}

void NativeFd::close() {
  if (!*this) {
    return;
  }

#if TD_FD_DEBUG
  get_fd_set().on_close_fd(fd());
#endif

  VLOG(fd) << *this << " close";
#if TD_PORT_WINDOWS
  if (is_socket_ ? closesocket(socket()) : !CloseHandle(fd())) {
#elif TD_PORT_POSIX
  if (::close(fd()) < 0) {
#endif
    auto error = OS_ERROR("Close fd");
    LOG(ERROR) << error;
  }
  fd_ = {};
}

NativeFd::Fd NativeFd::release() {
  VLOG(fd) << *this << " release";
  auto res = fd_.get();
  fd_ = {};
  return res;
}

StringBuilder &operator<<(StringBuilder &sb, const NativeFd &fd) {
  return sb << tag("fd", fd.fd());
}

}  // namespace td
