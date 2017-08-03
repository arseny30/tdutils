#include "td/utils/port/config.h"

char disable_linker_warning_about_empty_file_file_fd_cpp TD_UNUSED;

#ifdef TD_PORT_POSIX

#include "td/utils/format.h"
#include "td/utils/port/FileFd.h"
#include "td/utils/port/Stat.h"

#include <fcntl.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/file.h>
#include <sys/uio.h>

#include <cstring>

namespace td {

FileFd::operator FdRef() {
  return fd_;
}

/*** FileFd ***/
Result<FileFd> FileFd::open(CSlice filepath, int32 flags, int32 mode) {
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
    return Status::Error(PSLICE() << "Failed to open file: unknown flags. " << tag("path", filepath)
                                  << tag("flags", initial_flags));
  }

  int native_fd;
  while (true) {
    native_fd = ::open(filepath.c_str(), native_flags, static_cast<mode_t>(mode));
    auto open_errno = errno;
    if (native_fd == -1) {
      if (open_errno == EINTR) {
        continue;
      }

      return Status::PosixError(
          open_errno, PSLICE() << "Failed to open file: " << tag("path", filepath) << tag("flags", initial_flags));
    }
    break;
  }

  FileFd result;
  result.fd_ = Fd(native_fd, Fd::Mode::Own);
  result.fd_.update_flags(Fd::Flag::Write);
  return std::move(result);
}

Result<size_t> FileFd::write(Slice slice) {
  CHECK(!fd_.empty());
  int native_fd = get_native_fd();
  while (true) {
    auto write_res = ::write(native_fd, slice.begin(), slice.size());
    auto write_errno = errno;
    if (write_res < 0) {
      if (write_errno == EINTR) {
        continue;
      }

      auto error = Status::PosixError(write_errno, PSLICE() << "Write to [fd = " << native_fd << "] has failed");
      if (write_errno != EAGAIN
#if EAGAIN != EWOULDBLOCK
          && write_errno != EWOULDBLOCK
#endif
          && write_errno != EIO) {
        LOG(ERROR) << error;
      }
      return std::move(error);
    }
    return static_cast<size_t>(write_res);
  }
}

Result<size_t> FileFd::read(MutableSlice slice) {
  CHECK(!fd_.empty());
  int native_fd = get_native_fd();
  while (true) {
    auto read_res = ::read(native_fd, slice.begin(), slice.size());
    auto read_errno = errno;
    if (read_res < 0) {
      if (read_errno == EINTR) {
        continue;
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
    }
    if (static_cast<size_t>(read_res) < slice.size()) {
      fd_.clear_flags(Read);
    }
    return static_cast<size_t>(read_res);
  }
}

Result<size_t> FileFd::pwrite(Slice slice, off_t offset) {
  CHECK(!fd_.empty());
  int native_fd = get_native_fd();
  while (true) {
    auto pwrite_res = ::pwrite(native_fd, slice.begin(), slice.size(), offset);
    auto pwrite_errno = errno;
    if (pwrite_res < 0) {
      if (pwrite_errno == EINTR) {
        continue;
      }

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
    }
    return static_cast<size_t>(pwrite_res);
  }
}

Result<size_t> FileFd::pread(MutableSlice slice, off_t offset) {
  CHECK(!fd_.empty());
  int native_fd = get_native_fd();
  while (true) {
    auto pread_res = ::pread(native_fd, slice.begin(), slice.size(), offset);
    auto pread_errno = errno;
    if (pread_res < 0) {
      if (pread_errno == EINTR) {
        continue;
      }

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
    }
    return static_cast<size_t>(pread_res);
  }
}

Status FileFd::lock(FileFd::LockFlags flags, int32 max_tries) {
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
}

void FileFd::close() {
  fd_.close();
}

bool FileFd::empty() const {
  return fd_.empty();
}

int FileFd::get_native_fd() const {
  return fd_.get_native_fd();
}

int32 FileFd::get_flags() const {
  return fd_.get_flags();
}

void FileFd::update_flags(Fd::Flags mask) {
  fd_.update_flags(mask);
}

Status FileFd::get_pending_error() {
  return Status::OK();
}

off_t FileFd::get_size() {
  return detail::fstat(get_native_fd()).size_;
}

Fd FileFd::move_as_fd() {
  return std::move(fd_);
}

Stat FileFd::stat() {
  return fd_.stat();
}

Status FileFd::sync() {
  auto err = fsync(fd_.get_native_fd());
  if (err < 0) {
    return Status::OsError("Sync failed");
  }
  return Status::OK();
}

}  // namespace td

#endif  // TD_PORT_POSIX
