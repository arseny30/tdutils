#include "td/utils/port/path.h"

#ifdef TD_PORT_POSIX

#if TD_GCC
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#endif
#include <sys/stat.h>
#if TD_GCC
#pragma GCC diagnostic pop
#endif
#include <unistd.h>

#include <sys/types.h>

#include <climits>
#include <cstdio>
#include <cstdlib>

#endif

namespace td {

static string temporary_dir;

Status set_temporary_dir(CSlice dir) {
  string input_dir = dir.str();
  if (!dir.empty() && dir.back() != TD_DIR_SLASH) {
    input_dir += TD_DIR_SLASH;
  }
  TRY_STATUS(mkpath(input_dir, 0750));
  TRY_RESULT(real_dir, realpath(input_dir));
  temporary_dir = std::move(real_dir);
  return Status::OK();
}

#ifdef TD_PORT_POSIX

Status mkdir(CSlice dir, int32 mode) {
  int err = ::mkdir(dir.c_str(), static_cast<mode_t>(mode));
  auto mkdir_errno = errno;
  if (err < 0 && mkdir_errno != EEXIST) {
    return Status::PosixError(mkdir_errno, PSTR() << "Can't create directory \"" << dir << '"');
  }
  return Status::OK();
}

Status mkpath(CSlice path, int32 mode) {
  for (size_t i = 1; i < path.size(); i++) {
    if (path[i] == TD_DIR_SLASH) {
      TRY_STATUS(mkdir(path.substr(0, i).str(), mode))
    }
  }
  return Status::OK();
}

Status rename(CSlice from, CSlice to) {
  int err = ::rename(from.c_str(), to.c_str());
  if (err < 0) {
    auto rename_errno = errno;
    return Status::PosixError(rename_errno, PSTR() << "Can't rename \"" << from << "\" to \"" << to << '\"');
  }
  return Status::OK();
}

Result<string> realpath(CSlice slice) {
  char full_path[PATH_MAX + 1];
  char *err = ::realpath(slice.c_str(), full_path);
  if (err != full_path) {
    auto realpath_errno = errno;
    return Status::PosixError(realpath_errno, PSTR() << "Realpath failed for \"" << slice << '"');
  }
  string res = full_path;
  if (res.empty()) {
    return Status::Error("Empty path");
  }
  if (!slice.empty() && slice.end()[-1] == TD_DIR_SLASH) {
    if (res.back() != TD_DIR_SLASH) {
      res += TD_DIR_SLASH;
    }
  }
  return res;
}

Status chdir(CSlice dir) {
  if (::chdir(dir.c_str())) {
    auto chdir_errno = errno;
    return Status::PosixError(chdir_errno, PSTR() << "Can't change directory to \"" << dir << '"');
  }
  return Status::OK();
}

Status rmdir(CSlice dir) {
  if (::rmdir(dir.c_str())) {
    auto rmdir_errno = errno;
    return Status::PosixError(rmdir_errno, PSTR() << "Can't delete directory \"" << dir << '"');
  }
  return Status::OK();
}

Status unlink(CSlice path) {
  if (::unlink(path.c_str())) {
    auto unlink_errno = errno;
    return Status::PosixError(unlink_errno, PSTR() << "Can't unlink \"" << path << '"');
  }
  return Status::OK();
}

CSlice get_temporary_dir() {
  static bool is_inited = [] {
    if (temporary_dir.empty()) {
      const char *s = getenv("TMPDIR");
      if (s != nullptr && s[0] != '\0') {
        temporary_dir = s;
      } else if (P_tmpdir != nullptr && P_tmpdir[0] != '\0') {
        temporary_dir = P_tmpdir;
      } else {
        return false;
      }
    }
    if (temporary_dir.size() > 1 && temporary_dir.back() == TD_DIR_SLASH) {
      temporary_dir.pop_back();
    }
    return true;
  }();
  LOG_IF(FATAL, !is_inited) << "Can't find temporary directory";
  return temporary_dir;
}

Result<std::pair<FileFd, string>> mkstemp(CSlice dir) {
  if (dir.empty()) {
    dir = get_temporary_dir();
    if (dir.empty()) {
      return Status::Error("Can't find temporary directory");
    }
  }

  TRY_RESULT(dir_real, realpath(dir));
  CHECK(!dir_real.empty());

  string file_pattern;
  file_pattern.reserve(dir_real.size() + 14);
  file_pattern = dir_real;
  if (file_pattern.back() != TD_DIR_SLASH) {
    file_pattern += TD_DIR_SLASH;
  }
  file_pattern += "tmpXXXXXXXXXX";

  int fd = ::mkstemp(&file_pattern[0]);
  if (fd == -1) {
    auto mkstemp_errno = errno;
    return Status::PosixError(mkstemp_errno, PSTR() << "Can't create temporary file \"" << file_pattern << '"');
  }
  if (close(fd)) {
    auto close_errno = errno;
    return Status::PosixError(close_errno, PSTR() << "Can't close temporary file \"" << file_pattern << '"');
  }
  // TODO create file from fd
  TRY_RESULT(file, FileFd::open(file_pattern, FileFd::Write | FileFd::Truncate | FileFd::Append));
  return std::make_pair(std::move(file), std::move(file_pattern));
}

Result<string> mkdtemp(CSlice dir, Slice prefix) {
  if (dir.empty()) {
    dir = get_temporary_dir();
    if (dir.empty()) {
      return Status::Error("Can't find temporary directory");
    }
  }

  TRY_RESULT(dir_real, realpath(dir));
  CHECK(!dir_real.empty());

  string dir_pattern;
  dir_pattern.reserve(dir_real.size() + prefix.size() + 7);
  dir_pattern = dir_real;
  if (dir_pattern.back() != TD_DIR_SLASH) {
    dir_pattern += TD_DIR_SLASH;
  }
  dir_pattern.append(prefix.begin(), prefix.size());
  dir_pattern += "XXXXXX";

  char *result = ::mkdtemp(&dir_pattern[0]);
  if (result == nullptr) {
    auto mkdtemp_errno = errno;
    return Status::PosixError(mkdtemp_errno, PSTR() << "Can't create temporary directory \"" << dir_pattern << '"');
  }
  return result;
}

#endif  // TD_PORT_POSIX

#ifdef TD_PORT_WINDOWS

Status mkdir(CSlice dir, int32 mode) {
  TRY_RESULT(wdir, to_wstring(dir));
  auto status = CreateDirectoryW(wdir.c_str(), nullptr);
  if (status == 0 && GetLastError() != ERROR_ALREADY_EXISTS) {
    return Status::OsError(PSTR() << "Can't create directory \"" << dir << '"');
  }
  return Status::OK();
}

Status mkpath(CSlice path, int32 mode) {
  // TODO(perf): convert to wstring first
  for (size_t i = 1; i < path.size(); i++) {
    if (path[i] == TD_DIR_SLASH) {
      TRY_STATUS(mkdir(path.substr(0, i).str(), mode));
    }
  }
  return Status::OK();
}

Status rename(CSlice from, CSlice to) {
  TRY_RESULT(wfrom, to_wstring(from));
  TRY_RESULT(wto, to_wstring(to));
  auto status = MoveFileExW(wfrom.c_str(), wto.c_str(), 0);
  if (status == 0) {
    return Status::OsError(PSTR() << "Can't rename \"" << from << "\" to \"" << to << '\"');
  }
  return Status::OK();
}

Result<string> realpath(CSlice slice) {
  wchar_t buf[MAX_PATH + 1];
  TRY_RESULT(wslice, to_wstring(slice));
  auto status = GetFullPathNameW(wslice.c_str(), MAX_PATH, buf, nullptr);
  if (status == 0) {
    return Status::OsError(PSTR() << "GetFullPathNameW failed for \"" << slice << '"');
  }
  TRY_RESULT(res, to_string(buf));
  if (res.empty()) {
    return Status::Error("Empty path");
  }
  if (!slice.empty() && slice.end()[-1] == TD_DIR_SLASH) {
    if (res.back() != TD_DIR_SLASH) {
      res += TD_DIR_SLASH;
    }
  }
  return res;
}

Status chdir(CSlice dir) WARN_UNUSED_RESULT {
  TRY_RESULT(wdir, to_wstring(dir));
  auto res = SetCurrentDirectoryW(wdir.c_str());
  if (res == 0) {
    return Status::OsError(PSTR() << "Can't change directory to \"" << dir << '"');
  }
  return Status::OK();
}

Status rmdir(CSlice dir) {
  TRY_RESULT(wdir, to_wstring(dir));
  int status = RemoveDirectoryW(wdir.c_str());
  if (!status) {
    return Status::OsError(PSTR() << "Can't delete directory \"" << dir << '"');
  }
  return Status::OK();
}

Status unlink(CSlice path) {
  TRY_RESULT(wpath, to_wstring(path));
  int status = DeleteFileW(wpath.c_str());
  if (!status) {
    return Status::OsError(PSTR() << "Can't unlink \"" << path << '"');
  }
  return Status::OK();
}

CSlice get_temporary_dir() {
  static bool is_inited = [] {
    if (temporary_dir.empty()) {
      wchar_t buf[MAX_PATH + 1];
      auto status = GetTempPathW(MAX_PATH, buf);
      LOG_IF(FATAL, status == 0) << Status::OsError("GetTempPathW failed");
      auto rs = to_string(buf);
      LOG_IF(FATAL, rs.is_error()) << "GetTempPathW failed: " << rs.error();
      temporary_dir = rs.ok();
    }
    if (temporary_dir.size() > 1 && temporary_dir.back() == TD_DIR_SLASH) {
      temporary_dir.pop_back();
    }
    return true;
  }();
  LOG_IF(FATAL, !is_inited) << "Can't find temporary directory";
  return temporary_dir;
}

Result<string> mkdtemp(CSlice dir, Slice prefix) {
  if (dir.empty()) {
    dir = get_temporary_dir();
    if (dir.empty()) {
      return Status::Error("Can't find temporary directory");
    }
  }

  TRY_RESULT(dir_real, realpath(dir));
  CHECK(!dir_real.empty());

  string dir_pattern;
  dir_pattern.reserve(dir_real.size() + prefix.size() + 7);
  dir_pattern = dir_real;
  if (dir_pattern.back() != TD_DIR_SLASH) {
    dir_pattern += TD_DIR_SLASH;
  }
  dir_pattern.append(prefix.begin(), prefix.size());

  for (auto it = 0; it < 20; it++) {
    auto path = dir_pattern;
    for (int i = 0; i < 6 + i / 5; i++) {
      path += rand_fast_uint32() % 26 + 'a';
    }
    auto status = mkdir(path);
    if (status.is_ok()) {
      return path;
    }
  }
  return Status::Error(PSTR() << "Can't create temporary directory \"" << dir_pattern << '"');
}

Result<std::pair<FileFd, string>> mkstemp(CSlice dir) {
  if (dir.empty()) {
    dir = get_temporary_dir();
    if (dir.empty()) {
      return Status::Error("Can't find temporary directory");
    }
  }

  TRY_RESULT(dir_real, realpath(dir));
  CHECK(!dir_real.empty());

  string file_pattern;
  file_pattern.reserve(dir_real.size() + 14);
  file_pattern = dir_real;
  if (file_pattern.back() != TD_DIR_SLASH) {
    file_pattern += TD_DIR_SLASH;
  }
  file_pattern += "tmp";

  for (auto it = 0; it < 20; it++) {
    auto path = file_pattern;
    for (int i = 0; i < 6 + i / 5; i++) {
      path += rand_fast_uint32() % 26 + 'a';
    }
    auto r_file = FileFd::open(path, FileFd::Write | FileFd::Read | FileFd::CreateNew);
    if (r_file.is_ok()) {
      return std::make_pair(r_file.move_as_ok(), path);
    }
  }

  return Status::Error(PSTR() << "Can't create temporary file \"" << file_pattern << '"');
}

#endif  // TD_PORT_WINDOWS

}  // end of namespace td
