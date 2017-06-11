#include "td/utils/port/Fd.h"

#include "td/utils/port/config.h"

#ifdef TD_PORT_POSIX

#include "td/utils/port/Fd.h"

#include "td/utils/common.h"
#include "td/utils/format.h"
#include "td/utils/misc.h"

#include <atomic>

#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

#endif

#ifdef TD_PORT_WINDOWS

#include "td/utils/buffer.h"
#include "td/utils/format.h"
#include "td/utils/misc.h"

#include <algorithm>
#include <cstring>

#endif

namespace td {

#ifdef TD_PORT_POSIX

Fd::InfoSet::InfoSet() {
  get_info(0).refcnt = 1;
  get_info(1).refcnt = 1;
  get_info(2).refcnt = 1;
}
Fd::Info &Fd::InfoSet::get_info(int32 id) {
  CHECK(0 <= id && id < InfoSet::MAX_FD) << tag("fd", id);
  return fd_array_[id];
}
Fd::InfoSet Fd::fd_info_set_;

// TODO(bug) if constuctor call tries to output something to the LOG it will fail, because log is not initialized
Fd Fd::stderr_(2, Mode::Reference);
Fd Fd::stdout_(1, Mode::Reference);
Fd Fd::stdin_(0, Mode::Reference);

/*** Fd ***/
Fd::Fd() = default;

// TODO: separate class for FdRef!
Fd::Fd(int fd, Mode mode, ObserverBase *observer) : fd_(fd), mode_(mode) {
  auto *info = get_info();
  int old_ref_cnt = info->refcnt.load(std::memory_order_relaxed);
  if (old_ref_cnt == 0) {
    old_ref_cnt = info->refcnt.load(std::memory_order_acquire);
    CHECK(old_ref_cnt == 0);
    CHECK(mode_ == Mode::Own) << tag("fd", fd_);
    VLOG(fd) << "FD created [fd:" << fd_ << "]";

    auto fcntl_res = fcntl(fd_, F_GETFD);
    auto fcntl_errno = errno;
    LOG_IF(FATAL, fcntl_res == -1) << Status::PosixError(fcntl_errno, "fcntl F_GET_FD failed");

    info->refcnt.store(1, std::memory_order_relaxed);
    CHECK(!is_ref());
    CHECK(info->observer == nullptr);
    info->flags = 0;
    info->observer = observer;
  } else {
    CHECK(mode_ == Mode::Reference) << tag("fd", fd_);
    auto fcntl_res = fcntl(fd_, F_GETFD);
    auto fcntl_errno = errno;
    LOG_IF(FATAL, fcntl_res == -1) << Status::PosixError(fcntl_errno, "fcntl F_GET_FD failed");

    CHECK(is_ref());
    CHECK(observer == nullptr);
    // VLOG(fd) << "FdRef created [fd:" << fd_ << "]";
    info->refcnt.fetch_add(1, std::memory_order_relaxed);
  }
}

int Fd::move_as_native_fd() {
  clear_info();
  auto res = fd_;
  fd_ = -1;
  return res;
}

Fd::~Fd() {
  close();
}

Fd::Fd(Fd &&other) {
  fd_ = other.fd_;
  mode_ = other.mode_;
  other.fd_ = -1;
}

Fd &Fd::operator=(Fd &&other) {
  // with this check this may be equal &other
  CHECK(empty());
  fd_ = other.fd_;
  mode_ = other.mode_;
  other.fd_ = -1;
  return *this;
}

Fd Fd::clone() {
  return Fd(fd_, Mode::Reference);
}

Fd &Fd::Stderr() {
  return stderr_;
}
Fd &Fd::Stdout() {
  return stdout_;
}
Fd &Fd::Stdin() {
  return stdin_;
}

bool Fd::empty() const {
  return fd_ == -1;
}

const Fd &Fd::get_fd() const {
  return *this;
}

Fd &Fd::get_fd() {
  return *this;
}

int Fd::get_native_fd() const {
  CHECK(!empty());
  return fd_;
}

void Fd::set_observer(ObserverBase *observer) {
  auto *info = get_info();
  CHECK(observer == nullptr || info->observer == nullptr);
  info->observer = observer;
}

ObserverBase *Fd::get_observer() const {
  auto *info = get_info();
  return info->observer;
}

void Fd::close_ref() {
  CHECK(mode_ == Mode::Reference);
  auto *info = get_info();

  int old_ref_cnt = info->refcnt.fetch_sub(1, std::memory_order_relaxed);
  CHECK(old_ref_cnt > 1) << tag("fd", fd_);
  fd_ = -1;
}

void Fd::close_own() {
  CHECK(mode_ == Mode::Own);
  VLOG(fd) << "FD closed [fd:" << fd_ << "]";

  clear_info();
  ::close(fd_);
  fd_ = -1;
}

void Fd::close() {
  if (!empty()) {
    switch (mode_) {
      case Mode::Reference:
        close_ref();
        break;
      case Mode::Own:
        close_own();
        break;
    }
  }
}

Fd::Info *Fd::get_info() {
  CHECK(!empty());
  return &fd_info_set_.get_info(fd_);
}

const Fd::Info *Fd::get_info() const {
  CHECK(!empty());
  return &fd_info_set_.get_info(fd_);
}

void Fd::clear_info() {
  CHECK(!empty());
  CHECK(!is_ref());

  auto *info = get_info();
  int old_ref_cnt = info->refcnt.load(std::memory_order_relaxed);
  CHECK(old_ref_cnt == 1);
  info->flags = 0;
  info->observer = nullptr;
  info->refcnt.store(0, std::memory_order_release);
}

void Fd::update_flags_notify(Flags flags) {
  update_flags_inner(flags, true);
}

void Fd::update_flags(Flags flags) {
  update_flags_inner(flags, false);
}

void Fd::update_flags_inner(int32 new_flags, bool notify_flag) {
  if (new_flags & Error) {
    new_flags |= Error;
    new_flags |= Close;
  }
  auto *info = get_info();
  int32 &flags = info->flags;
  int32 old_flags = flags;
  flags |= new_flags;
  if (new_flags & Close) {
    // TODO: ???
    flags &= ~Write;
  }
  if (flags != old_flags) {
    VLOG(fd) << "Update flags " << tag("fd", fd_) << tag("from", format::as_binary(old_flags))
             << tag("to", format::as_binary(flags));
  }
  if (flags != old_flags && notify_flag) {
    auto observer = info->observer;
    if (observer != nullptr) {
      observer->notify();
    }
  }
}

bool Fd::can_read() const {
  return get_flags() & Fd::Read;
}
bool Fd::can_write() const {
  return get_flags() & Fd::Write;
}
bool Fd::can_close() const {
  return get_flags() & Fd::Close;
}

int32 Fd::get_flags() const {
  return get_info()->flags;
}

void Fd::clear_flags(Flags flags) {
  get_info()->flags &= ~flags;
}

bool Fd::has_pending_error() const {
  return get_flags() & Fd::Error;
}
Status Fd::get_pending_error() {
  if (!(get_flags() & Fd::Error)) {
    return Status::OK();
  }
  clear_flags(Fd::Error);
  return Status::Error(PSLICE() << "Can't load error on generic fd [fd_=" << get_native_fd() << "]");
}

Result<size_t> Fd::write_unsafe(Slice slice) {
  ssize_t ssize;
  int native_fd = get_native_fd();
  while (true) {
    ssize = ::write(native_fd, slice.begin(), slice.size());
    auto write_errno = errno;
    if (ssize < 0) {
      if (write_errno == EINTR) {
        continue;
      }
      return Status::PosixError(write_errno, PSLICE("Write to [fd=%d] has failed", native_fd));
    }
    break;
  }
  return static_cast<size_t>(ssize);
}

Result<size_t> Fd::write(Slice slice) {
  ssize_t ssize;
  int native_fd = get_native_fd();
  while (true) {
    ssize = ::write(native_fd, slice.begin(), slice.size());
    auto write_errno = errno;
    if (ssize < 0) {
      if (write_errno == EINTR) {
        continue;
      }
      if (write_errno == EAGAIN
#if EAGAIN != EWOULDBLOCK
          || write_errno == EWOULDBLOCK
#endif
          ) {
        clear_flags(Write);
        return 0;
      }

      auto error = Status::PosixError(write_errno, PSLICE("Write to [fd=%d] has failed", native_fd));
      switch (write_errno) {
        case EBADF:
        case ENXIO:
        case EFAULT:
        case EINVAL:
          LOG(FATAL) << error;
        default:
          LOG(WARNING) << error;
        // no break
        case ECONNRESET:
        case EDQUOT:
        case EFBIG:
        case EIO:
        case ENETDOWN:
        case ENETUNREACH:
        case ENOSPC:
        case EPIPE:
          clear_flags(Write);
          update_flags(Close);
          return std::move(error);
      }
    }
    break;
  }
  return static_cast<size_t>(ssize);
}

Result<size_t> Fd::read(MutableSlice slice) {
  ssize_t ssize;
  int native_fd = get_native_fd();
  while (true) {
    CHECK(slice.size() > 0);
    ssize = ::read(native_fd, slice.begin(), slice.size());
    auto read_errno = errno;
    if (ssize < 0) {
      if (read_errno == EINTR) {
        continue;
      }

      if (read_errno == EAGAIN
#if EAGAIN != EWOULDBLOCK
          || read_errno == EWOULDBLOCK
#endif
          ) {
        clear_flags(Read);
        return 0;
      }
      auto error = Status::PosixError(read_errno, PSLICE("Read from [fd=%d] has failed", native_fd));
      switch (read_errno) {
        case EISDIR:
        case EBADF:
        case ENXIO:
        case EFAULT:
        case EINVAL:
        case ENOTCONN:
          LOG(FATAL) << error;

        default:
          LOG(WARNING) << error;
        // no break
        case EIO:
        case ENOBUFS:
        case ENOMEM:
        case ECONNRESET:
        case ETIMEDOUT:
          clear_flags(Read);
          update_flags(Close);
          return std::move(error);
      }
    }
    break;
  }
  if (ssize == 0) {
    errno = 0;
    clear_flags(Read);
    update_flags(Close);
  }
  return static_cast<size_t>(ssize);
}

Stat Fd::stat() const {
  CHECK(!empty());
  return detail::fstat(get_native_fd());
}

Status Fd::set_is_blocking(bool is_blocking) {
  auto old_flags = fcntl(fd_, F_GETFL);
  if (old_flags == -1) {
    auto fcntl_errno = errno;
    return Status::PosixError(fcntl_errno, "Failed to get socket flags");
  }
  auto new_flags = is_blocking ? old_flags | O_NONBLOCK : old_flags & ~O_NONBLOCK;
  if (new_flags != old_flags && fcntl(fd_, F_SETFL, new_flags) == -1) {
    auto fcntl_errno = errno;
    return Status::PosixError(fcntl_errno, "Failed to set socket flags");
  }

  return Status::OK();
}

#endif  // TD_PORT_POSIX

#ifdef TD_PORT_WINDOWS

namespace detail {
class FdImpl {
 public:
  FdImpl(Fd::Type type, HANDLE handle)
      : type_(type), handle_(handle), async_mode_(type_ == Fd::Type::EventFd || type_ == Fd::Type::StdinFileFd) {
    init();
  }
  FdImpl(Fd::Type type, SOCKET sock, int32 socket_family)
      : type_(type), socket_(sock), socket_family_(socket_family), async_mode_(true) {
    init();
  }

  FdImpl(const FdImpl &) = delete;
  FdImpl &operator=(const FdImpl &) = delete;
  FdImpl(FdImpl &&) = delete;
  FdImpl &operator=(FdImpl &&) = delete;

  ~FdImpl() {
    close();
  }
  void set_observer(ObserverBase *observer) {
    observer_ = observer;
  }
  ObserverBase *get_observer() const {
    return observer_;
  }

  void update_flags_notify(Fd::Flags flags) {
    update_flags_inner(flags, true);
  }

  void update_flags(Fd::Flags flags) {
    update_flags_inner(flags, false);
  }

  void update_flags_inner(int32 new_flags, bool notify_flag) {
    if (new_flags & Fd::Error) {
      new_flags |= Fd::Error;
      new_flags |= Fd::Close;
    }
    int32 old_flags = flags_;
    flags_ |= new_flags;
    if (new_flags & Fd::Close) {
      // TODO: ???
      flags_ &= ~Fd::Write;
      internal_flags_ &= ~Fd::Write;
    }
    if (flags_ != old_flags) {
      VLOG(fd) << "Update flags " << tag("fd", get_io_handle()) << tag("from", format::as_binary(old_flags))
               << tag("to", format::as_binary(flags_));
    }
    if (flags_ != old_flags && notify_flag) {
      auto observer = get_observer();
      if (observer != nullptr) {
        observer->notify();
      }
    }
  }

  int32 get_flags() const {
    return flags_;
  }

  void clear_flags(Fd::Flags mask) {
    flags_ &= ~mask;
  }

  Status get_pending_error() {
    if (!(get_flags() & Fd::Error)) {
      return Status::OK();
    }
    clear_flags(Fd::Error);
    return std::move(pending_error_);
  }
  bool has_pending_error() const {
    return (get_flags() & Fd::Flag::Error) != 0;
  }

  HANDLE get_read_event() {
    if (type() == Fd::Type::StdinFileFd) {
      return get_io_handle();
    }
    return read_event_;
  }
  void on_read_event() {
    if (type_ == Fd::Type::StdinFileFd) {
      return try_read_stdin();
    }
    ResetEvent(read_event_);
    if (type_ == Fd::Type::EventFd) {
      return update_flags_notify(Fd::Flag::Read);
    }
    if (type_ == Fd::Type::SocketFd && !connected_) {
      on_connect_ready();
    } else {
      if (!async_read_flag_) {
        return;
      }

      if (type_ == Fd::Type::ServerSocketFd) {
        on_accept_ready();
      } else {
        on_read_ready();
      }
    }
    loop();
  }
  HANDLE get_write_event() {
    return write_event_;
  }
  void on_write_event() {
    CHECK(async_write_flag_);
    ResetEvent(write_event_);
    on_write_ready();
    loop();
  }

  Result<size_t> write(Slice slice) WARN_UNUSED_RESULT {
    if (async_mode_) {
      return write_async(slice);
    } else {
      return write_sync(slice);
    }
  }

  Result<size_t> read(MutableSlice slice) WARN_UNUSED_RESULT {
    if (async_mode_) {
      return read_async(slice);
    } else {
      return read_sync(slice);
    }
  }

  Result<size_t> write_async(Slice slice) WARN_UNUSED_RESULT {
    CHECK(async_mode_);
    output_writer_.append(slice);
    output_reader_.sync_with_writer();
    loop();
    return slice.size();
  }
  Result<size_t> write_sync(Slice slice) WARN_UNUSED_RESULT {
    CHECK(!async_mode_);
    DWORD bytes_written = 0;
    auto res = WriteFile(get_io_handle(), slice.data(), narrow_cast<DWORD>(slice.size()), &bytes_written, nullptr);
    if (!res) {
      return Status::OsError("Failed to write_sync");
    }
    return bytes_written;
  }
  Result<size_t> read_async(MutableSlice slice) WARN_UNUSED_RESULT {
    CHECK(async_mode_);
    auto res = input_reader_.advance(std::min(slice.size(), input_reader_.size()), slice);
    if (res == 0) {
      clear_flags(Fd::Flag::Read);
    }
    return res;
  }
  Result<size_t> read_sync(MutableSlice slice) WARN_UNUSED_RESULT {
    CHECK(!async_mode_);
    DWORD bytes_read = 0;
    auto res = ReadFile(get_io_handle(), slice.data(), narrow_cast<DWORD>(slice.size()), &bytes_read, nullptr);
    if (!res) {
      return Status::OsError("Failed to read_sync");
    }
    if (bytes_read == 0) {
      clear_flags(Fd::Flag::Read);
    }
    return bytes_read;
  }

  // for ServerSocket
  Result<Fd> accept() {
    if (accepted_.empty()) {
      clear_flags(Fd::Flag::Read);
      return Status::PosixError<EAGAIN>();  // TODO: do not export system specific errors.
    }
    auto res = std::move(accepted_.back());
    accepted_.pop_back();
    return std::move(res);
  }

  void connect(const IPAddress &addr) {
    CHECK(!connected_);
    CHECK(type_ == Fd::Type::SocketFd);
    DWORD bytes_read;
    std::memset(&read_overlapped_, 0, sizeof(read_overlapped_));
    read_overlapped_.hEvent = read_event_;
    LPFN_CONNECTEX ConnectExPtr = nullptr;
    GUID guid = WSAID_CONNECTEX;
    DWORD numBytes;
    int error = ::WSAIoctl(socket_, SIO_GET_EXTENSION_FUNCTION_POINTER, static_cast<void *>(&guid), sizeof(guid),
                           static_cast<void *>(&ConnectExPtr), sizeof(ConnectExPtr), &numBytes, nullptr, nullptr);
    if (error) {
      return on_error(Status::WsaError("WSAIoctl failed"), Fd::Flag::Read);
    }
    auto status = ConnectExPtr(socket_, addr.get_sockaddr(), narrow_cast<int>(addr.get_sockaddr_len()), nullptr, 0,
                               &bytes_read, &read_overlapped_);
    if (status != 0) {
      ResetEvent(read_event_);
      connected_ = true;
      update_flags_notify(Fd::Flag::Read);
      return;
    }

    auto last_error = GetLastError();
    if (last_error == ERROR_IO_PENDING) {
      return;
    }
    on_error(Status::WsaError("ConnectEx failed"), Fd::Flag::Read);
  }

  // for EventFd
  void release() {
    CHECK(type_ == Fd::Type::EventFd);
    SetEvent(read_event_);
  }

  void acquire() {
    CHECK(type_ == Fd::Type::EventFd);
    ResetEvent(read_event_);
    clear_flags(Fd::Flag::Read);
  }

  Result<size_t> pwrite(Slice slice, off_t pos) WARN_UNUSED_RESULT {
    DWORD bytes_written = 0;
    OVERLAPPED overlapped;
    std::memset(&overlapped, 0, sizeof(overlapped));
    auto pos64 = static_cast<uint64>(pos);
    overlapped.Offset = static_cast<DWORD>(pos64);
    overlapped.OffsetHigh = static_cast<DWORD>(pos64 >> 32);
    auto res = WriteFile(get_io_handle(), slice.data(), narrow_cast<DWORD>(slice.size()), &bytes_written, &overlapped);
    if (!res) {
      return Status::OsError("Failed to write_sync");
    }
    return bytes_written;
  }

  Result<size_t> pread(MutableSlice slice, off_t pos) WARN_UNUSED_RESULT {
    DWORD bytes_read = 0;
    OVERLAPPED overlapped;
    std::memset(&overlapped, 0, sizeof(overlapped));
    auto pos64 = static_cast<uint64>(pos);
    overlapped.Offset = static_cast<DWORD>(pos64);
    overlapped.OffsetHigh = static_cast<DWORD>(pos64 >> 32);
    auto res = ReadFile(get_io_handle(), slice.data(), narrow_cast<DWORD>(slice.size()), &bytes_read, &overlapped);
    if (!res) {
      return Status::OsError("Failed to write_sync");
    }
    return bytes_read;
  }

  // TODO: interface for BufferedFd optimization.

  bool empty() const {
    return type() == Fd::Type::Empty;
  }
  void close() {
    if (empty()) {
      return;
    }
    switch (type()) {
      case Fd::Type::StdinFileFd:
      case Fd::Type::FileFd: {
        auto res = CloseHandle(handle_);
        LOG_IF(ERROR, !res) << Status::OsError("Failed to close file");
        handle_ = INVALID_HANDLE_VALUE;
        break;
      }
      case Fd::Type::ServerSocketFd:
      case Fd::Type::SocketFd: {
        auto res = closesocket(socket_);
        LOG_IF(ERROR, res != 0) << Status::OsError("Failed to close socket");
        socket_ = INVALID_SOCKET;
        break;
      }
      case Fd::Type::EventFd:
        break;
      default:
        UNREACHABLE();
    }

    if (read_event_ != INVALID_HANDLE_VALUE) {
      auto res = CloseHandle(read_event_);
      LOG_IF(ERROR, !res) << Status::OsError("Failed to close event");
      read_event_ = INVALID_HANDLE_VALUE;
    }
    if (write_event_ != INVALID_HANDLE_VALUE) {
      auto res = CloseHandle(write_event_);
      LOG_IF(ERROR, !res) << Status::OsError("Failed to close event");
      write_event_ = INVALID_HANDLE_VALUE;
    }

    type_ = Fd::Type::Empty;
  }

  Stat stat() const {
    CHECK(type_ == Fd::Type::FileFd);
    Stat res;

    FILE_BASIC_INFO basic_info;
    auto status = GetFileInformationByHandleEx(get_io_handle(), FileBasicInfo, &basic_info, sizeof(basic_info));
    LOG_IF(FATAL, !status) << Status::OsError("Stat failed");
    res.atime_nsec_ = basic_info.LastAccessTime.QuadPart * 100;
    res.mtime_nsec_ = basic_info.LastWriteTime.QuadPart * 100;
    res.is_dir_ = (basic_info.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    res.is_reg_ = true;

    FILE_STANDARD_INFO standard_info;
    status = GetFileInformationByHandleEx(get_io_handle(), FileStandardInfo, &standard_info, sizeof(standard_info));
    LOG_IF(FATAL, !status) << Status::OsError("Stat failed");
    res.size_ = narrow_cast<off_t>(standard_info.EndOfFile.QuadPart);

    return res;
  }

  off_t get_size() const {
    return stat().size_;
  }

  Status sync() const {
    CHECK(type_ == Fd::Type::FileFd);
    auto status = FlushFileBuffers(get_io_handle());
    if (status) {
      return Status::OK();
    }
    return Status::OsError("Sync failed");
  }

 private:
  Fd::Type type_;
  HANDLE handle_ = INVALID_HANDLE_VALUE;
  SOCKET socket_ = INVALID_SOCKET;

  int32 socket_family_ = 0;

  bool async_mode_ = false;

  ObserverBase *observer_ = nullptr;
  Fd::Flags flags_ = Fd::Flag::Write;
  Status pending_error_;
  Fd::Flags internal_flags_ = Fd::Flag::Write | Fd::Flag::Read;

  HANDLE read_event_ = INVALID_HANDLE_VALUE;  // used by WineventPoll
  bool async_read_flag_ = false;              // do we have pending read?
  OVERLAPPED read_overlapped_;
  ChainBufferWriter input_writer_;
  ChainBufferReader input_reader_ = input_writer_.extract_reader();

  bool connected_ = false;

  std::vector<Fd> accepted_;
  SOCKET accept_socket_ = INVALID_SOCKET;
  static constexpr size_t MAX_ADDR_SIZE = sizeof(sockaddr_in6) + 16;
  char addr_buf_[MAX_ADDR_SIZE * 2];

  HANDLE write_event_ = INVALID_HANDLE_VALUE;  // used by WineventPoll
  bool async_write_flag_ = false;              // do we have pending write?
  OVERLAPPED write_overlapped_;
  ChainBufferWriter output_writer_;
  ChainBufferReader output_reader_ = output_writer_.extract_reader();

  void init() {
    if (async_mode_) {
      if (type_ != Fd::Type::EventFd) {
        write_event_ = CreateEventW(nullptr, true, false, nullptr);
      }
      read_event_ = CreateEventW(nullptr, true, false, nullptr);
      loop();
    }
  }

  Fd::Type type() const {
    return type_;
  }

  HANDLE get_io_handle() const {
    CHECK(!empty());
    if (type() == Fd::Type::FileFd || type() == Fd::Type::StdinFileFd) {
      return handle_;
    }
    return reinterpret_cast<HANDLE>(socket_);
  }

  void on_error(Status error, Fd::Flag flag) {
    VLOG(fd) << tag("fd", get_io_handle()) << error;
    pending_error_ = std::move(error);
    internal_flags_ &= ~flag;
    update_flags_notify(Fd::Flag::Error);
  }
  void on_eof() {
    internal_flags_ &= ~Fd::Flag::Read;
    update_flags_notify(Fd::Flag::Close);
  }

  void on_read_ready() {
    async_read_flag_ = false;
    DWORD bytes_read;
    auto status = GetOverlappedResult(get_io_handle(), &read_overlapped_, &bytes_read, false);
    if (status == 0) {
      return on_error(Status::OsError("ReadFile failed"), Fd::Flag::Read);
    }

    VLOG(fd) << "Read " << tag("fd", get_io_handle()) << tag("size", bytes_read);
    if (bytes_read == 0) {  // eof
      return on_eof();
    }
    input_writer_.confirm_append(bytes_read);
    input_reader_.sync_with_writer();
    update_flags_notify(Fd::Flag::Read);
  }
  void on_write_ready() {
    async_write_flag_ = false;
    DWORD bytes_written;
    auto status = GetOverlappedResult(get_io_handle(), &write_overlapped_, &bytes_written, false);
    if (status == 0) {
      return on_error(Status::OsError("WriteFile failed"), Fd::Flag::Write);
    }
    if (bytes_written != 0) {
      VLOG(fd) << "Write " << tag("fd", get_io_handle()) << tag("size", bytes_written);
      output_reader_.confirm_read(bytes_written);
      update_flags_notify(Fd::Flag::Write);
    }
  }

  void on_accept_ready() {
    async_read_flag_ = false;
    DWORD bytes_read;
    auto status = GetOverlappedResult(get_io_handle(), &read_overlapped_, &bytes_read, false);
    if (status == 0) {
      return on_error(Status::OsError("AcceptEx failed"), Fd::Flag::Write);
    }
    accepted_.push_back(Fd(Fd::Type::SocketFd, Fd::Mode::Owner, accept_socket_));
    accept_socket_ = INVALID_SOCKET;
    update_flags_notify(Fd::Flag::Read);
  }

  void on_connect_ready() {
    async_read_flag_ = false;
    DWORD bytes_read;
    VLOG(fd) << "on_connect_ready";
    auto status = GetOverlappedResult(get_io_handle(), &read_overlapped_, &bytes_read, false);
    if (status == 0) {
      return on_error(Status::OsError("ConnectEx failed"), Fd::Flag::Write);
    }
    connected_ = true;
    VLOG(fd) << "connected = true";
  }

  void try_read_stdin() {
  }
  void try_start_read() {
    auto dest = input_writer_.prepare_append();
    DWORD bytes_read;
    std::memset(&read_overlapped_, 0, sizeof(read_overlapped_));
    read_overlapped_.hEvent = read_event_;
    VLOG(fd) << "try_read..";
    auto status =
        ReadFile(get_io_handle(), dest.data(), narrow_cast<DWORD>(dest.size()), &bytes_read, &read_overlapped_);
    if (status != 0) {  // ok
      ResetEvent(read_event_);
      VLOG(fd) << "Read " << tag("fd", get_io_handle()) << tag("size", bytes_read);
      if (bytes_read == 0) {  // eof
        return on_eof();
      }
      input_writer_.confirm_append(bytes_read);
      input_reader_.sync_with_writer();
      update_flags_notify(Fd::Flag::Read);
      return;
    }
    auto last_error = GetLastError();
    if (last_error == ERROR_IO_PENDING) {
      async_read_flag_ = true;
      return;
    }
    on_error(Status::OsError("ReadFile failed"), Fd::Flag::Read);
  }

  void try_start_write() {
    auto dest = output_reader_.prepare_read();
    DWORD bytes_written;
    std::memset(&write_overlapped_, 0, sizeof(write_overlapped_));
    write_overlapped_.hEvent = write_event_;
    VLOG(fd) << "try_start_write";
    auto status =
        WriteFile(get_io_handle(), dest.data(), narrow_cast<DWORD>(dest.size()), &bytes_written, &write_overlapped_);
    if (status != 0) {  // ok
      VLOG(fd) << "Write " << tag("fd", get_io_handle()) << tag("size", bytes_written);
      ResetEvent(write_event_);
      output_reader_.confirm_read(bytes_written);
      update_flags_notify(Fd::Flag::Write);
      return;
    }
    auto last_error = GetLastError();
    if (last_error == ERROR_IO_PENDING) {
      VLOG(fd) << "try_start_write: ERROR_IO_PENDING";
      async_write_flag_ = true;
      return;
    }
    CHECK(WaitForSingleObject(write_event_, 0) != WAIT_OBJECT_0);
    on_error(Status::OsError("WriteFile failed"), Fd::Flag::Write);
  }

  void try_start_accept() {
    if (async_read_flag_ == true) {
      return;
    }
    accept_socket_ = socket(socket_family_, SOCK_STREAM, 0);
    DWORD bytes_read;
    std::memset(&read_overlapped_, 0, sizeof(read_overlapped_));
    read_overlapped_.hEvent = read_event_;
    auto status =
        AcceptEx(socket_, accept_socket_, addr_buf_, 0, MAX_ADDR_SIZE, MAX_ADDR_SIZE, &bytes_read, &read_overlapped_);
    if (status != 0) {
      ResetEvent(read_event_);
      accepted_.push_back(Fd(Fd::Type::SocketFd, Fd::Mode::Owner, accept_socket_));
      accept_socket_ = INVALID_SOCKET;
      update_flags_notify(Fd::Flag::Read);
      return;
    }

    auto last_error = GetLastError();
    if (last_error == ERROR_IO_PENDING) {
      async_read_flag_ = true;
      return;
    }
    on_error(Status::OsError("AcceptExFailed"), Fd::Flag::Read);
  }

  void loop() {
    CHECK(async_mode_);

    if (type_ == Fd::Type::EventFd) {
      return;
    }
    if (type_ == Fd::Type::ServerSocketFd) {
      while (async_read_flag_ == false && (internal_flags_ & Fd::Flag::Read) != 0) {
        // read always
        try_start_accept();
      }
      return;
    }
    if (!connected_) {
      return;
    }
    while (async_read_flag_ == false && (internal_flags_ & Fd::Flag::Read) != 0) {
      // read always
      try_start_read();
    }
    VLOG(fd) << (async_write_flag_ == false) << " " << output_reader_.size() << " "
             << ((internal_flags_ & Fd::Flag::Write) != 0);
    while (async_write_flag_ == false && output_reader_.size() && (internal_flags_ & Fd::Flag::Write) != 0) {
      // write if we have data to write
      try_start_write();
    }
  }
};

}  // namespace detail

Result<size_t> Fd::write(Slice slice) {
  CHECK(!empty());
  return impl_->write(slice);
}
bool Fd::empty() const {
  return !impl_;
}
void Fd::close() {
  impl_.reset();
}

Result<size_t> Fd::read(MutableSlice slice) {
  return impl_->read(slice);
}
Result<size_t> Fd::pwrite(Slice slice, off_t pos) {
  return impl_->pwrite(slice, pos);
}
Result<size_t> Fd::pread(MutableSlice slice, off_t pos) {
  return impl_->pread(slice, pos);
}

Result<Fd> Fd::accept() {
  return impl_->accept();
}
void Fd::connect(const IPAddress &addr) {
  return impl_->connect(addr);
}

Fd Fd::clone() const {
  return Fd(impl_);
}

uint64 Fd::get_key() const {
  return reinterpret_cast<uint64>(impl_.get());
}

void Fd::set_observer(ObserverBase *observer) {
  return impl_->set_observer(observer);
}
ObserverBase *Fd::get_observer() const {
  return impl_->get_observer();
}

int32 Fd::get_flags() const {
  return impl_->get_flags();
}
void Fd::update_flags(Flags flags) {
  impl_->update_flags(flags);
}
bool Fd::can_read() const {
  return (get_flags() & Flag::Read) != 0;
}
bool Fd::can_write() const {
  return (get_flags() & Flag::Write) != 0;
}
bool Fd::can_close() const {
  return (get_flags() & Flag::Close) != 0;
}
// void Fd::update_flags_notify(Flags flags) {
//  impl_->update_flags_notify(flags);
//}
void Fd::on_read_event() {
  impl_->on_read_event();
}
void Fd::on_write_event() {
  impl_->on_write_event();
}

bool Fd::has_pending_error() const {
  return impl_->has_pending_error();
}
Status Fd::get_pending_error() {
  return impl_->get_pending_error();
}

HANDLE Fd::get_read_event() {
  return impl_->get_read_event();
}
HANDLE Fd::get_write_event() {
  return impl_->get_write_event();
}

#if !TD_WINDOWS || WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
Fd Fd::Stderr() {
  static auto handle = GetStdHandle(STD_ERROR_HANDLE);
  LOG_IF(FATAL, handle == INVALID_HANDLE_VALUE) << "Failed to get stderr";
  static auto fd = Fd(Fd::Type::FileFd, Fd::Mode::Reference, handle);
  return fd.clone();
}
Fd Fd::Stdin() {
  static auto handle = GetStdHandle(STD_INPUT_HANDLE);
  LOG_IF(FATAL, handle == INVALID_HANDLE_VALUE) << "Failed to get stdin";
  static auto fd = Fd(Fd::Type::FileFd, Fd::Mode::Reference, handle);
  return fd.clone();
}
Fd Fd::Stdout() {
  static auto handle = GetStdHandle(STD_OUTPUT_HANDLE);
  LOG_IF(FATAL, handle == INVALID_HANDLE_VALUE) << "Failed to get stdout";
  static auto fd = Fd(Fd::Type::FileFd, Fd::Mode::Reference, handle);
  return fd.clone();
}
#else
Fd Fd::Stderr() {
  return Fd();
}
Fd Fd::Stdin() {
  return Fd();
}
Fd Fd::Stdout() {
  return Fd();
}
#endif

Fd::Fd(Type type, Mode mode, HANDLE handle) : mode_(mode), impl_(make_shared<detail::FdImpl>(type, handle)) {
}
Fd::Fd(Type type, Mode mode, SOCKET sock, int32 socket_family)
    : mode_(mode), impl_(make_shared<detail::FdImpl>(type, sock, socket_family)) {
}
Fd::Fd(shared_ptr<detail::FdImpl> impl) : mode_(Mode::Reference), impl_(std::move(impl)) {
}
void Fd::acquire() {
  return impl_->acquire();
}
void Fd::release() {
  return impl_->release();
}

Stat Fd::stat() const {
  return impl_->stat();
}
off_t Fd::get_size() const {
  return impl_->get_size();
}
Status Fd::sync() {
  return impl_->sync();
}

class InitWSA {
 public:
  InitWSA() {
    /* Use the MAKEWORD(lowbyte, highbyte) macro declared in Windef.h */
    WORD wVersionRequested = MAKEWORD(2, 2);
    WSADATA wsaData;
    int err = WSAStartup(wVersionRequested, &wsaData);
    LOG_IF(FATAL, err != 0) << Status::WsaError("Failed to init WSA");
  }
};
static InitWSA init_wsa;
#endif

}  // namespace td
