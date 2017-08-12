#include "td/utils/port/config.h"

#include "td/utils/port/FileFd.h"

#ifdef TD_PORT_POSIX
#include <fcntl.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include <cstring>
#endif

namespace td {

const Fd &FileFd::get_fd() const {
  return fd_;
}

Fd &FileFd::get_fd() {
  return fd_;
}

Result<FileFd> FileFd::open(CSlice filepath, int32 flags, int32 mode) {
#ifdef TD_PORT_POSIX
  int32 initial_flags = flags;
  int native_flags = 0;

  if ((flags & Write) && (flags & Read)) {
    native_flags |= O_RDWR;
  } else if (flags & Write) {
    native_flags |= O_WRONLY;
  } else if (flags & Read) {
    native_flags |= O_RDONLY;
  } else {
    return Status::Error(PSLICE() << "Failed to open file: invalid flags. [path=" << filepath
                                  << "] [flags=" << initial_flags << "]");
  }
  flags &= ~(Write | Read);

  if (flags & Truncate) {
    native_flags |= O_TRUNC;
    flags &= ~Truncate;
  }

  if (flags & Create) {
    native_flags |= O_CREAT;
    flags &= ~Create;
  } else if (flags & CreateNew) {
    native_flags |= O_CREAT;
    native_flags |= O_EXCL;
    flags &= ~CreateNew;
  }

  if (flags & Append) {
    native_flags |= O_APPEND;
    flags &= ~Append;
  }

  if (flags) {
    return Status::Error(PSLICE() << "Failed to open file \"" << filepath << "\" with unknown flags " << initial_flags);
  }

  int native_fd = skip_eintr([&] { return ::open(filepath.c_str(), native_flags, static_cast<mode_t>(mode)); });
  if (native_fd < 0) {
    auto open_errno = errno;
    return Status::PosixError(open_errno,
                              PSLICE() << "Failed to open file \"" << filepath << "\" with flags " << initial_flags);
  }

  FileFd result;
  result.fd_ = Fd(native_fd, Fd::Mode::Own);
  result.fd_.update_flags(Fd::Flag::Write);
  return std::move(result);
#endif
#ifdef TD_PORT_WINDOWS
  // TODO: support modes
  auto r_filepath = to_wstring(filepath);
  if (r_filepath.is_error()) {
    return Status::Error(PSLICE() << "Failed to convert file path \" << filepath << \" to UTF-16");
  }
  auto w_filepath = r_filepath.move_as_ok();
  DWORD desired_access = 0;
  if ((flags & Write) && (flags & Read)) {
    desired_access |= GENERIC_READ | GENERIC_WRITE;
  } else if (flags & Write) {
    desired_access |= GENERIC_WRITE;
  } else if (flags & Read) {
    desired_access |= GENERIC_READ;
  } else {
    return Status::Error(PSLICE() << "Failed to open file \"" << filepath << "\": invalid flags " << flags);
  }
  flags &= ~(Write | Read);

  // TODO: share mode
  DWORD share_mode = FILE_SHARE_READ | FILE_SHARE_DELETE | FILE_SHARE_WRITE;

  DWORD creation_disposition = 0;
  if (flags & Create) {
    flags &= ~Create;
    if (flags & Truncate) {
      flags &= ~Truncate;
      creation_disposition = CREATE_ALWAYS;
    } else {
      creation_disposition = OPEN_ALWAYS;
    }
  } else if (flags & CreateNew) {
    flags &= ~CreateNew;
    if (flags & Truncate) {
      flags &= ~Truncate;
      creation_disposition = CREATE_NEW;
    } else {
      creation_disposition = CREATE_NEW;
    }
  } else {
    if (flags & Truncate) {
      flags &= ~Truncate;
      creation_disposition = TRUNCATE_EXISTING;
    } else {
      creation_disposition = OPEN_EXISTING;
    }
  }

  bool append_flag = false;
  if (flags & Append) {
    append_flag = true;
    flags &= ~Append;
  }

  if (flags) {
    return Status::Error(PSLICE() << "Failed to open file \"" << filepath << "\" with unknown flags " << flags);
  }

  auto handle = CreateFile2(w_filepath.c_str(), desired_access, share_mode, creation_disposition, nullptr);
  if (handle == INVALID_HANDLE_VALUE) {
    return Status::OsError(PSLICE() << "Failed to open file \"" << filepath << "\" with flags " << flags);
  }
  if (append_flag) {
    LARGE_INTEGER offset;
    offset.QuadPart = 0;
    auto set_pointer_res = SetFilePointerEx(handle, offset, nullptr, FILE_END);
    if (!set_pointer_res) {
      auto res = Status::OsError(PSLICE() << "Failed to seek to the end of file \"" << filepath << "\"");
      CloseHandle(handle);
      return res;
    }
  }
  FileFd result;
  result.fd_ = Fd(Fd::Type::FileFd, Fd::Mode::Owner, handle);
  result.fd_.update_flags(Fd::Flag::Write);
  return std::move(result);
#endif
}

Result<size_t> FileFd::write(Slice slice) {
#ifdef TD_PORT_POSIX
  CHECK(!fd_.empty());
  int native_fd = get_native_fd();
  auto write_res = skip_eintr([&] { return ::write(native_fd, slice.begin(), slice.size()); });
  if (write_res >= 0) {
    return static_cast<size_t>(write_res);
  }

  auto write_errno = errno;
  auto error = Status::PosixError(write_errno, PSLICE() << "Write to [fd = " << native_fd << "] has failed");
  if (write_errno != EAGAIN
#if EAGAIN != EWOULDBLOCK
      && write_errno != EWOULDBLOCK
#endif
      && write_errno != EIO) {
    LOG(ERROR) << error;
  }
  return std::move(error);
#endif
#ifdef TD_PORT_WINDOWS
  return fd_.write(slice);
#endif
}

Result<size_t> FileFd::read(MutableSlice slice) {
#ifdef TD_PORT_POSIX
  CHECK(!fd_.empty());
  int native_fd = get_native_fd();
  auto read_res = skip_eintr([&] { return ::read(native_fd, slice.begin(), slice.size()); });
  auto read_errno = errno;

  if (read_res >= 0) {
    if (static_cast<size_t>(read_res) < slice.size()) {
      fd_.clear_flags(Read);
    }
    return static_cast<size_t>(read_res);
  }

  auto error = Status::PosixError(read_errno, PSLICE() << "Read from [fd = " << native_fd << "] has failed");
  if (read_errno != EAGAIN
#if EAGAIN != EWOULDBLOCK
      && read_errno != EWOULDBLOCK
#endif
      && read_errno != EIO) {
    LOG(ERROR) << error;
  }
  return std::move(error);
#endif
#ifdef TD_PORT_WINDOWS
  return fd_.read(slice);
#endif
}

Result<size_t> FileFd::pwrite(Slice slice, off_t offset) {
#ifdef TD_PORT_POSIX
  CHECK(!fd_.empty());
  int native_fd = get_native_fd();
  auto pwrite_res = skip_eintr([&] { return ::pwrite(native_fd, slice.begin(), slice.size(), offset); });
  if (pwrite_res >= 0) {
    return static_cast<size_t>(pwrite_res);
  }

  auto pwrite_errno = errno;
  auto error = Status::PosixError(
      pwrite_errno, PSLICE() << "Pwrite to [fd = " << native_fd << "] at [offset = " << offset << "] has failed");
  if (pwrite_errno != EAGAIN
#if EAGAIN != EWOULDBLOCK
      && read_errno != EWOULDBLOCK
#endif
      && pwrite_errno != EIO) {
    LOG(ERROR) << error;
  }
  return std::move(error);
#endif
#ifdef TD_PORT_WINDOWS
  return fd_.pwrite(slice, offset);
#endif
}

Result<size_t> FileFd::pread(MutableSlice slice, off_t offset) {
#ifdef TD_PORT_POSIX
  CHECK(!fd_.empty());
  int native_fd = get_native_fd();
  auto pread_res = skip_eintr([&] { return ::pread(native_fd, slice.begin(), slice.size(), offset); });
  if (pread_res >= 0) {
    return static_cast<size_t>(pread_res);
  }

  auto pread_errno = errno;
  auto error = Status::PosixError(
      pread_errno, PSLICE() << "Pread from [fd = " << native_fd << "] at [offset = " << offset << "] has failed");
  if (pread_errno != EAGAIN
#if EAGAIN != EWOULDBLOCK
      && read_errno != EWOULDBLOCK
#endif
      && pread_errno != EIO) {
    LOG(ERROR) << error;
  }
  return std::move(error);
#endif
#ifdef TD_PORT_WINDOWS
  return fd_.pread(slice, offset);
#endif
}

Status FileFd::lock(FileFd::LockFlags flags, int32 max_tries) {
#ifdef TD_PORT_POSIX
  if (max_tries <= 0) {
    return Status::Error(0, "Can't lock file: wrong max_tries");
  }

  while (true) {
    struct flock L;
    std::memset(&L, 0, sizeof(L));

    L.l_type = static_cast<short>([&] {
      switch (flags) {
        case LockFlags::Read:
          return F_RDLCK;
        case LockFlags::Write:
          return F_WRLCK;
        case LockFlags::Unlock:
          return F_UNLCK;
        default:
          UNREACHABLE();
          return F_UNLCK;
      };
    }());

    L.l_whence = SEEK_SET;
    if (fcntl(fd_.get_native_fd(), F_SETLK, &L) == -1) {
      int fcntl_errno = errno;
      if (fcntl_errno == EAGAIN && --max_tries > 0) {
        usleep(100000);
        continue;
      }

      return Status::PosixError(fcntl_errno, "Can't lock file");
    }
    return Status::OK();
  }
#endif
#ifdef TD_PORT_WINDOWS
  return Status::OK();  // TODO(now)
#endif
}

void FileFd::close() {
  fd_.close();
}

bool FileFd::empty() const {
  return fd_.empty();
}

#ifdef TD_PORT_POSIX
int FileFd::get_native_fd() const {
  return fd_.get_native_fd();
}
#endif

int32 FileFd::get_flags() const {
  return fd_.get_flags();
}

void FileFd::update_flags(Fd::Flags mask) {
  fd_.update_flags(mask);
}

off_t FileFd::get_size() {
#ifdef TD_PORT_POSIX
  return detail::fstat(get_native_fd()).size_;
#endif
#ifdef TD_PORT_WINDOWS
  return fd_.get_size();
#endif
}

Fd FileFd::move_as_fd() {
#ifdef TD_PORT_POSIX
  return std::move(fd_);
#endif
#ifdef TD_PORT_WINDOWS
  Fd res = fd_.clone();
  // TODO(now): fix ownership
  return std::move(res);
#endif
}

Stat FileFd::stat() {
  return fd_.stat();
}

Status FileFd::sync() {
#ifdef TD_PORT_POSIX
  auto err = fsync(fd_.get_native_fd());
  if (err < 0) {
    return Status::OsError("Sync failed");
  }
  return Status::OK();
#endif
#ifdef TD_PORT_WINDOWS
  return fd_.sync();
#endif
}

}  // namespace td
