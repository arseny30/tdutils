#pragma once

#include "td/utils/port/config.h"

#ifdef TD_PORT_POSIX

#include "td/utils/port/Fd.h"
#include "td/utils/Slice.h"
#include "td/utils/Status.h"

namespace td {
class FileFd {
 public:
  FileFd() = default;

  operator FdRef();

  enum Flags : int32 { Write = 1, Read = 2, Truncate = 4, Create = 8, Append = 16, CreateNew = 32 };

  static Result<FileFd> open(CSlice filepath, int32 flags, int32 mode = 0600) WARN_UNUSED_RESULT;

  Result<size_t> write(Slice slice) WARN_UNUSED_RESULT;
  Result<size_t> read(MutableSlice slice) WARN_UNUSED_RESULT;

  Result<size_t> pwrite(Slice slice, off_t offset) WARN_UNUSED_RESULT;
  Result<size_t> pread(MutableSlice slice, off_t offset) WARN_UNUSED_RESULT;

  enum class LockFlags { Write, Read, Unlock };
  Status lock(LockFlags flags) WARN_UNUSED_RESULT;

  void close();
  bool empty() const;

  int get_native_fd() const;
  int32 get_flags() const;
  void update_flags(Fd::Flags mask);

  Status get_pending_error() WARN_UNUSED_RESULT;

  off_t get_size();

  Fd move_as_fd();

  Stat stat();

  Status sync() WARN_UNUSED_RESULT;

 private:
  Fd fd_;
};
}  // namespace td
#endif  // TD_PORT_POSIX

#ifdef TD_PORT_WINDOWS
#include "td/utils/port/Fd.h"

namespace td {
// Just proxy to Fd.
// Exists for better type safety, and as a factory
class FileFd : public Fd {
 public:
  FileFd() = default;

  enum Flags : int32 { Write = 1, Read = 2, Truncate = 4, Create = 8, Append = 16, CreateNew = 32 };

  // TODO: support modes
  static Result<FileFd> open(CSlice filepath, int32 flags, int32 todo = 0) WARN_UNUSED_RESULT {
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
      return Status::Error(PSLICE() << "Failed to open file \"" << filepath << "\": unknown flags " << flags);
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
    auto res = FileFd(Fd::Type::FileFd, Fd::Mode::Owner, handle);
    res.update_flags(Fd::Flag::Write);
    return std::move(res);
  }

  using Fd::write;
  using Fd::read;
  using Fd::pwrite;
  using Fd::pread;
  using Fd::close;
  using Fd::empty;
  using Fd::get_flags;
  using Fd::has_pending_error;
  using Fd::get_pending_error;
  using Fd::stat;
  using Fd::get_size;

  enum class LockFlags { Write, Read, Unlock };
  Status lock(LockFlags flags) WARN_UNUSED_RESULT {
    return Status::OK();
  }

  Fd move_as_fd() {
    Fd res = clone();
    // TODO(now): fix ownership
    return std::move(res);
  }

 private:
  using Fd::Fd;
};
}  // namespace td
#endif  // PORT_WINDOWS
