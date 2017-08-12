#pragma once

#include "td/utils/port/config.h"

#include "td/utils/common.h"
#include "td/utils/port/Fd.h"
#include "td/utils/port/Stat.h"
#include "td/utils/Slice.h"
#include "td/utils/Status.h"

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

#ifdef TD_PORT_POSIX
  int get_native_fd() const;
#endif  // TD_PORT_POSIX

 private:
  Fd fd_;
};

}  // namespace td
