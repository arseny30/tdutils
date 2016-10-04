#pragma once
#include "td/utils/port/config.h"
#ifdef TD_PORT_POSIX

#include "td/utils/common.h"
#include "td/utils/Status.h"
#include "td/utils/Slice.h"

namespace td {
struct Stat {
  bool is_dir_;
  bool is_reg_;
  off_t size_;
  uint64 atime_nsec_;
  uint64 mtime_nsec_;
};

namespace detail {
Stat from_native_stat(const struct stat *buf);
Stat fstat(int native_fd);
Status update_atime(int native_fd) WARN_UNUSED_RESULT;
}  // end of namespace detail

Result<Stat> stat(CSlice path) WARN_UNUSED_RESULT;
Status update_atime(CSlice path) WARN_UNUSED_RESULT;

struct MemStat {
  uint64 resident_size_;
  uint64 resident_size_peak_;
  uint64 virtual_size_;
  uint64 virtual_size_peak_;
};

Result<MemStat> mem_stat() WARN_UNUSED_RESULT;

}  // end of namespace td
#endif  // TD_PORT_POSIX

#ifdef TD_PORT_WINDOWS

#include "td/utils/common.h"
#include "td/utils/Status.h"
#include "td/utils/Slice.h"

namespace td {
struct Stat {
  bool is_dir_;
  bool is_reg_;
  off_t size_;
  uint64 atime_nsec_;
  uint64 mtime_nsec_;
};

Result<Stat> stat(CSlice path) WARN_UNUSED_RESULT;
inline Status update_atime(CSlice path) {
  return Status::Error("Not supported");
}

}  // namespace td

#endif  // TD_PORT_WINDOWS
