#include "td/utils/port/config.h"

#include "td/utils/port/FileFd.h"

#ifdef TD_PORT_POSIX
#include <fcntl.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#endif

#ifdef TD_PORT_WINDOWS
#include "td/utils/misc.h"  // for narrow_cast
#endif

#include <cstring>

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
    return Status::Error(PSLICE() << "Failed to open file \"" << filepath << "\" with invalid flags " << flags);
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
    return OS_ERROR(PSLICE() << "Failed to open file \"" << filepath << "\" with flags " << initial_flags);
  }

  FileFd result;
  result.fd_ = Fd(native_fd, Fd::Mode::Owner);
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
    return Status::Error(PSLICE() << "Failed to open file \"" << filepath << "\" with invalid flags " << flags);
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
    flags &= ~Truncate;
    creation_disposition = CREATE_NEW;
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
    return OS_ERROR(PSLICE() << "Failed to open file \"" << filepath << "\" with flags " << flags);
  }
  if (append_flag) {
    LARGE_INTEGER offset;
    offset.QuadPart = 0;
    auto set_pointer_res = SetFilePointerEx(handle, offset, nullptr, FILE_END);
    if (!set_pointer_res) {
      auto res = OS_ERROR(PSLICE() << "Failed to seek to the end of file \"" << filepath << "\"");
      CloseHandle(handle);
      return res;
    }
  }
  FileFd result;
  result.fd_ = Fd::create_file_fd(handle);
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
      && pwrite_errno != EWOULDBLOCK
#endif
      && pwrite_errno != EIO) {
    LOG(ERROR) << error;
  }
  return std::move(error);
#endif
#ifdef TD_PORT_WINDOWS
  DWORD bytes_written = 0;
  OVERLAPPED overlapped;
  std::memset(&overlapped, 0, sizeof(overlapped));
  auto pos64 = static_cast<uint64>(offset);
  overlapped.Offset = static_cast<DWORD>(pos64);
  overlapped.OffsetHigh = static_cast<DWORD>(pos64 >> 32);
  auto res =
      WriteFile(fd_.get_io_handle(), slice.data(), narrow_cast<DWORD>(slice.size()), &bytes_written, &overlapped);
  if (!res) {
    return OS_ERROR("Failed to pwrite");
  }
  return bytes_written;
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
      && pread_errno != EWOULDBLOCK
#endif
      && pread_errno != EIO) {
    LOG(ERROR) << error;
  }
  return std::move(error);
#endif
#ifdef TD_PORT_WINDOWS
  DWORD bytes_read = 0;
  OVERLAPPED overlapped;
  std::memset(&overlapped, 0, sizeof(overlapped));
  auto pos64 = static_cast<uint64>(offset);
  overlapped.Offset = static_cast<DWORD>(pos64);
  overlapped.OffsetHigh = static_cast<DWORD>(pos64 >> 32);
  auto res = ReadFile(fd_.get_io_handle(), slice.data(), narrow_cast<DWORD>(slice.size()), &bytes_read, &overlapped);
  if (!res) {
    return OS_ERROR("Failed to pread");
  }
  return bytes_read;
#endif
}

Status FileFd::lock(FileFd::LockFlags flags, int32 max_tries) {
  if (max_tries <= 0) {
    return Status::Error(0, "Can't lock file: wrong max_tries");
  }

  while (true) {
#ifdef TD_PORT_POSIX
    struct flock lock;
    std::memset(&lock, 0, sizeof(lock));

    lock.l_type = static_cast<short>([&] {
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
      }
    }());

    lock.l_whence = SEEK_SET;
    if (fcntl(get_native_fd(), F_SETLK, &lock) == -1) {
      if (errno == EAGAIN && --max_tries > 0) {
        usleep(100000);
#endif
#ifdef TD_PORT_WINDOWS
    OVERLAPPED overlapped;
    std::memset(&overlapped, 0, sizeof(overlapped));

    BOOL result;
    if (flags == LockFlags::Unlock) {
      result = UnlockFileEx(fd_.get_io_handle(), 0, MAXDWORD, MAXDWORD, &overlapped);
    } else {
      DWORD dw_flags = LOCKFILE_FAIL_IMMEDIATELY;
      if (flags == LockFlags::Write) {
        dw_flags |= LOCKFILE_EXCLUSIVE_LOCK;
      }

      result = LockFileEx(fd_.get_io_handle(), dw_flags, 0, MAXDWORD, MAXDWORD, &overlapped);
    }

    if (!result) {
      if (GetLastError() == ERROR_LOCK_VIOLATION && --max_tries > 0) {
        Sleep(100);
#endif
        continue;
      }

      return OS_ERROR("Can't lock file");
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
  return stat().size_;
}

Stat FileFd::stat() {
  CHECK(!empty());
#ifdef TD_PORT_POSIX
  return detail::fstat(get_native_fd());
#endif
#ifdef TD_PORT_WINDOWS
  Stat res;

  FILE_BASIC_INFO basic_info;
  auto status = GetFileInformationByHandleEx(fd_.get_io_handle(), FileBasicInfo, &basic_info, sizeof(basic_info));
  if (!status) {
    auto error = OS_ERROR("Stat failed");
    LOG(FATAL) << error;
  }
  res.atime_nsec_ = basic_info.LastAccessTime.QuadPart * 100;
  res.mtime_nsec_ = basic_info.LastWriteTime.QuadPart * 100;
  res.is_dir_ = (basic_info.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
  res.is_reg_ = true;

  FILE_STANDARD_INFO standard_info;
  status = GetFileInformationByHandleEx(fd_.get_io_handle(), FileStandardInfo, &standard_info, sizeof(standard_info));
  if (!status) {
    auto error = OS_ERROR("Stat failed");
    LOG(FATAL) << error;
  }
  res.size_ = narrow_cast<off_t>(standard_info.EndOfFile.QuadPart);

  return res;
#endif
}

Status FileFd::sync() {
  CHECK(!empty());
#ifdef TD_PORT_POSIX
  if (fsync(fd_.get_native_fd()) != 0) {
#endif
#ifdef TD_PORT_WINDOWS
  if (FlushFileBuffers(fd_.get_io_handle()) == 0) {
#endif
    return OS_ERROR("Sync failed");
  }
  return Status::OK();
}

Status FileFd::seek(off_t position) {
  CHECK(!empty());
#ifdef TD_PORT_POSIX
  if (skip_eintr([&] { return ::lseek(fd_.get_native_fd(), position, SEEK_SET); }) < 0) {
#endif
#ifdef TD_PORT_WINDOWS
  LARGE_INTEGER offset;
  offset.QuadPart = position;
  if (SetFilePointerEx(fd_.get_io_handle(), offset, nullptr, FILE_BEGIN) == 0) {
#endif
    return OS_ERROR("Seek failed");
  }
  return Status::OK();
}

Status FileFd::truncate_to_current_position(off_t current_position) {
  CHECK(!empty());
#ifdef TD_PORT_POSIX
  if (skip_eintr([&] { return ::ftruncate(fd_.get_native_fd(), current_position); }) < 0) {
#endif
#ifdef TD_PORT_WINDOWS
  if (SetEndOfFile(fd_.get_io_handle()) == 0) {
#endif
    return OS_ERROR("Truncate failed");
  }
  return Status::OK();
}

}  // namespace td
