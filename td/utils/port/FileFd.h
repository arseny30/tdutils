#pragma once

#include "td/utils/port/config.h"

#include "td/utils/port/Fd.h"
#include "td/utils/Slice.h"
#include "td/utils/Status.h"

#ifdef TD_PORT_POSIX

namespace td {
class FileFd {
 public:
  FileFd() = default;

  enum Flags : int32 { Write = 1, Read = 2, Truncate = 4, Create = 8, Append = 16, CreateNew = 32 };

  const Fd &get_fd() const;
  Fd &get_fd();

  static Result<FileFd> open(CSlice filepath, int32 flags, int32 mode = 0600) WARN_UNUSED_RESULT;

  Result<size_t> write(Slice slice) WARN_UNUSED_RESULT;
  Result<size_t> read(MutableSlice slice) WARN_UNUSED_RESULT;

  Result<size_t> pwrite(Slice slice, off_t offset) WARN_UNUSED_RESULT;
  Result<size_t> pread(MutableSlice slice, off_t offset) WARN_UNUSED_RESULT;

  enum class LockFlags { Write, Read, Unlock };
  Status lock(LockFlags flags, int32 max_tries = 1) WARN_UNUSED_RESULT;

  void close();
  bool empty() const;

  int32 get_flags() const;
  void update_flags(Fd::Flags mask);

  off_t get_size();

  Fd move_as_fd();

  Stat stat();

  Status sync() WARN_UNUSED_RESULT;

  int get_native_fd() const;

 private:
  Fd fd_;
};
}  // namespace td
#endif  // TD_PORT_POSIX

#ifdef TD_PORT_WINDOWS

namespace td {
// Just proxy to Fd.
// Exists for better type safety, and as a factory
class FileFd : public Fd {
 public:
  FileFd() = default;

  enum Flags : int32 { Write = 1, Read = 2, Truncate = 4, Create = 8, Append = 16, CreateNew = 32 };

  using Fd::get_fd;

  static Result<FileFd> open(CSlice filepath, int32 flags, int32 todo = 0) WARN_UNUSED_RESULT;

  using Fd::write;
  using Fd::read;

  using Fd::pwrite;
  using Fd::pread;

  enum class LockFlags { Write, Read, Unlock };
  Status lock(LockFlags flags, int32 max_tries = 1) WARN_UNUSED_RESULT;

  using Fd::close;
  using Fd::empty;

  using Fd::get_flags;

  using Fd::get_size;

  Fd move_as_fd();

  using Fd::stat;

  using Fd::sync;

 private:
  using Fd::Fd;
};

}  // namespace td
#endif  // TD_PORT_WINDOWS
