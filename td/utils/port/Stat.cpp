#include "td/utils/port/config.h"

#include "td/utils/port/FileFd.h"
#include "td/utils/port/Stat.h"

#ifdef TD_PORT_POSIX

#include "td/utils/format.h"
#include "td/utils/misc.h"
#include "td/utils/port/Clocks.h"
#include "td/utils/ScopeGuard.h"

#if TD_MAC
#include <mach/mach.h>
#include <sys/time.h>
#endif

#if TD_GCC
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#endif
#include <sys/stat.h>
#if TD_GCC
#pragma GCC diagnostic pop
#endif

#if TD_ANDROID || TD_TIZEN
#include <sys/syscall.h>
#include <sys/stat.h>
#endif

#include <cinttypes>
#include <cstdio>
#include <cstring>

namespace td {
namespace detail {
Stat from_native_stat(const struct ::stat &buf) {
  Stat res;
#if TD_ANDROID || TD_LINUX || TD_TIZEN
#if defined(_BSD_SOURCE) || defined(_SVID_SOURCE) || _POSIX_C_SOURCE >= 200809L || _XOPEN_SOURCE >= 700
  res.mtime_nsec_ = buf.st_mtime * 1000000000ll + buf.st_mtim.tv_nsec;
  res.atime_nsec_ = buf.st_atime * 1000000000ll + buf.st_atim.tv_nsec;
#else
  res.mtime_nsec_ = buf.st_mtime * 1000000000ll + buf.st_mtimensec;
  res.atime_nsec_ = buf.st_atime * 1000000000ll + buf.st_atimensec;
#endif
#elif TD_MAC
  res.mtime_nsec_ = buf.st_mtimespec.tv_sec * 1000000000ll + buf.st_mtimespec.tv_nsec;  // khm
  res.atime_nsec_ = buf.st_atimespec.tv_sec * 1000000000ll + buf.st_atimespec.tv_nsec;
#else
#error "Unsupported OS"
#endif
  // TODO check for max size
  // sometimes stat.st_size is greater than off_t
  res.size_ = static_cast<off_t>(buf.st_size);
  res.is_dir_ = (buf.st_mode & S_IFMT) == S_IFDIR;
  res.is_reg_ = (buf.st_mode & S_IFMT) == S_IFREG;
  res.mtime_nsec_ /= 1000;
  res.mtime_nsec_ *= 1000;

  return res;
}

Stat fstat(int native_fd) {
  struct ::stat buf;
  int err = fstat(native_fd, &buf);
  auto fstat_errno = errno;
  LOG_IF(FATAL, err < 0) << "stat " << tag("fd", native_fd) << " failed: " << Status::PosixError(fstat_errno);
  return detail::from_native_stat(buf);
}

Status update_atime(int native_fd) {
#if TD_LINUX
  struct timespec times[2];
  // access time
  times[0].tv_nsec = UTIME_NOW;
  // modify time
  times[1].tv_nsec = UTIME_OMIT;
  // int err = utimensat(native_fd, nullptr, times, 0);
  int err = futimens(native_fd, times);
  if (err < 0) {
    auto futimens_errno = errno;
    auto status = Status::PosixError(futimens_errno, PSLICE() << "futimens " << tag("fd", native_fd));
    LOG(WARNING) << status;
    return status;
  }
  return Status::OK();
#elif TD_MAC
  auto info = fstat(native_fd);
  struct timeval upd[2];
  auto now = Clocks::system();
  // access time
  upd[0].tv_sec = (int32)now;
  upd[0].tv_usec = (int)((now - (int32)now) * 1000000);
  // modify time
  upd[1].tv_sec = static_cast<int32>(info.mtime_nsec_ / 1000000000ll);
  upd[1].tv_usec = static_cast<int32>(info.mtime_nsec_ % 1000000000ll / 1000);
  int err = futimes(native_fd, upd);
  if (err < 0) {
    auto futimes_errno = errno;
    auto status = Status::PosixError(futimes_errno, PSLICE() << "futimes " << tag("fd", native_fd));
    LOG(WARNING) << status;
    return status;
  }
  return Status::OK();
#elif TD_ANDROID || TD_TIZEN
  return Status::Error();
// NOT SUPPORTED...
// struct timespec times[2];
//// access time
// times[0].tv_nsec = UTIME_NOW;
//// modify time
// times[1].tv_nsec = UTIME_OMIT;
////  int err = syscall(__NR_utimensat, native_fd, nullptr, times, 0);
// int err = futimens(native_fd, times);
// if (err < 0) {
// auto futimens_errno = errno;
// auto status = Status::PosixError(futimens_errno, PSLICE() << "futimens " << tag("fd", native_fd));
// LOG(WARNING) << status;
// return status;
//}
// return Status::OK();
#else
#error "Unsupported OS"
#endif
}
}  // namespace detail

Status update_atime(CSlice path) {
  TRY_RESULT(file, FileFd::open(path, FileFd::Flags::Read));
  SCOPE_EXIT {
    file.close();
  };
  return detail::update_atime(file.get_native_fd());
}

Result<Stat> stat(CSlice path) {
  struct ::stat buf;
  int err = stat(path.c_str(), &buf);
  if (err < 0) {
    auto stat_errno = errno;
    return Status::PosixError(stat_errno, PSLICE() << "stat " << tag("file", path) << " failed: ");
  }
  return detail::from_native_stat(buf);
}

Result<MemStat> mem_stat() {
#if TD_MAC
  struct task_basic_info t_info;
  mach_msg_type_number_t t_info_count = TASK_BASIC_INFO_COUNT;

  if (KERN_SUCCESS != task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)&t_info, &t_info_count)) {
    return Status::Error("task_info failed");
  }
  MemStat res;
  res.resident_size_ = t_info.resident_size;
  res.virtual_size_ = t_info.virtual_size;
  res.resident_size_peak_ = 0;
  res.virtual_size_peak_ = 0;
  return res;
#endif  // TD_MAC

#if TD_LINUX || TD_ANDROID || TD_TIZEN
  TRY_RESULT(fd, FileFd::open("/proc/self/status", FileFd::Read));
  SCOPE_EXIT {
    fd.close();
  };

  constexpr int TMEM_SIZE = 10000;
  char mem[TMEM_SIZE];
  TRY_RESULT(size, fd.read(MutableSlice(mem, TMEM_SIZE - 1)));
  CHECK(size < TMEM_SIZE - 1);
  mem[size] = 0;

  const char *s = mem;
  MemStat res;
  memset(&res, 0, sizeof(res));
  while (*s) {
    const char *name_begin = s;
    while (*s != 0 && *s != '\n') {
      s++;
    }
    auto name_end = name_begin;
    while (isalpha(*name_end)) {
      name_end++;
    }
    Slice name(name_begin, name_end);

    uint64 *x = nullptr;
    if (name == "VmPeak") {
      x = &res.virtual_size_peak_;
    }
    if (name == "VmSize") {
      x = &res.virtual_size_;
    }
    if (name == "VmHWM") {
      x = &res.resident_size_peak_;
    }
    if (name == "VmRSS") {
      x = &res.resident_size_;
    }
    if (x != nullptr) {
      *x = (unsigned long long)-1;

      unsigned long long xx;
      if (name_end == s || name_end + 1 == s || sscanf(name_end + 1, "%llu", &xx) != 1) {
        LOG(ERROR) << "Failed to parse memory stats" << tag("line", Slice(name_begin, s))
                   << tag(":number", Slice(name_end, s));
      } else {
        xx *= 1024;
        *x = static_cast<uint64>(xx);
        // memory is in kB
      }
    }
    if (*s == 0) {
      break;
    }
    s++;
  }

  return res;
#endif
}
}  // namespace td
#endif  // TD_PORT_POSIX

#ifdef TD_PORT_WINDOWS
namespace td {

Result<Stat> stat(CSlice path) {
  TRY_RESULT(fd, FileFd::open(path, FileFd::Flags::Read));
  return fd.stat();
}

}  // namespace td
#endif  // TD_PORT_WINDOWS
