#include "td/utils/port/config.h"
#ifdef TD_PORT_POSIX

#include "td/utils/port/FileFd.h"
#include "td/utils/format.h"

#include <fcntl.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/file.h>
#include <sys/uio.h>

// We don't want warnings from system headers
#if TD_GCC
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#endif
#include <sys/stat.h>
#if TD_GCC
#pragma GCC diagnostic pop
#endif

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

  int native_fd = ::open(filepath.c_str(), native_flags, static_cast<mode_t>(mode));
  auto open_errno = errno;
  if (native_fd == -1) {
    return Status::PosixError(
        open_errno, PSLICE() << "Failed to open file: " << tag("path", filepath) << tag("flags", initial_flags));
  }

  FileFd result;
  result.fd_ = Fd(native_fd, Fd::Mode::Own);
  result.fd_.update_flags(Fd::Flag::Write);
  return std::move(result);
}

Result<size_t> FileFd::write(Slice slice) {
  CHECK(!fd_.empty());
  ssize_t ssize;
  int native_fd = get_native_fd();
  while (true) {
    ssize = ::write(native_fd, slice.begin(), slice.size());
    auto write_errno = errno;
    if (ssize < 0) {
      if (write_errno == EINTR) {
        continue;
      }

      auto error = Status::PosixError(write_errno, PSLICE() << "Write to [fd = " << native_fd << "] has failed");
      if (write_errno != EAGAIN && write_errno != EWOULDBLOCK && write_errno != EIO) {
        LOG(ERROR) << error;
      }
      return std::move(error);
    }
    break;
  }
  return static_cast<size_t>(ssize);
}

Result<size_t> FileFd::read(MutableSlice slice) {
  CHECK(!fd_.empty());
  ssize_t ssize;
  int native_fd = get_native_fd();
  while (true) {
    ssize = ::read(native_fd, slice.begin(), slice.size());
    auto read_errno = errno;
    if (ssize < 0) {
      if (read_errno == EINTR) {
        continue;
      }

      auto error = Status::PosixError(read_errno, PSLICE() << "Read from [fd = " << native_fd << "] has failed");
      if (read_errno != EAGAIN && read_errno != EWOULDBLOCK && read_errno != EIO) {
        LOG(ERROR) << error;
      }
      return std::move(error);
    }
    break;
  }
  if (static_cast<size_t>(ssize) < slice.size()) {
    fd_.clear_flags(Read);
  }
  return static_cast<size_t>(ssize);
}

Result<size_t> FileFd::pwrite(Slice slice, off_t offset) {
  CHECK(!fd_.empty());
  ssize_t ssize;
  int native_fd = get_native_fd();
  while (true) {
    ssize = ::pwrite(native_fd, slice.begin(), slice.size(), offset);
    auto pwrite_errno = errno;
    if (ssize < 0) {
      if (pwrite_errno == EINTR) {
        continue;
      }

      auto error = Status::PosixError(
          pwrite_errno, PSLICE() << "Pwrite to [fd = " << native_fd << "] at [offset = " << offset << "] has failed");
      if (pwrite_errno != EAGAIN && pwrite_errno != EWOULDBLOCK && pwrite_errno != EIO) {
        LOG(ERROR) << error;
      }
      return std::move(error);
    }
    break;
  }
  return static_cast<size_t>(ssize);
}

Result<size_t> FileFd::pread(MutableSlice slice, off_t offset) {
  CHECK(!fd_.empty());
  ssize_t ssize;
  int native_fd = get_native_fd();
  while (true) {
    ssize = ::pread(native_fd, slice.begin(), slice.size(), offset);
    auto pread_errno = errno;
    if (ssize < 0) {
      if (pread_errno == EINTR) {
        continue;
      }

      auto error = Status::PosixError(
          pread_errno, PSLICE() << "Pread from [fd = " << native_fd << "] at [offset = " << offset << "] has failed");
      if (pread_errno != EAGAIN && pread_errno != EWOULDBLOCK && pread_errno != EIO) {
        LOG(ERROR) << error;
      }
      return std::move(error);
    }
    break;
  }
  return static_cast<size_t>(ssize);
}

Status FileFd::lock(FileFd::LockFlags flags) {
  struct flock L;
  memset(&L, 0, sizeof(L));

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
    return Status::PosixError(fcntl_errno, "Can't lock file");
  }
  return Status::OK();
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
  struct stat buf;
  int err = fstat(get_native_fd(), &buf);
  auto fstat_errno = errno;
  LOG_IF(FATAL, err != 0) << "fstat failed " << tag("fd", get_native_fd()) << " " << Status::PosixError(fstat_errno);
  return static_cast<off_t>(buf.st_size);  // sometimes stat.st_size is greater than off_t
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
