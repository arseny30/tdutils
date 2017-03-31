#pragma once
#include "td/utils/port/config.h"

#include "td/utils/common.h"
#include "td/utils/ScopeGuard.h"
#include "td/utils/Slice.h"
#include "td/utils/Status.h"

#include "td/utils/port/FileFd.h"

#include <utility>

#ifdef TD_PORT_POSIX

#include "td/utils/format.h"

#include <sys/types.h>
#include <dirent.h>

#endif

namespace td {

Status mkdir(CSlice dir, int32 mode = 0700) WARN_UNUSED_RESULT;
Status mkpath(CSlice path, int32 mode = 0700) WARN_UNUSED_RESULT;
Status rename(CSlice from, CSlice to) WARN_UNUSED_RESULT;
Result<string> realpath(CSlice slice) WARN_UNUSED_RESULT;
Status chdir(CSlice dir) WARN_UNUSED_RESULT;
Status rmdir(CSlice dir) WARN_UNUSED_RESULT;
Status unlink(CSlice path) WARN_UNUSED_RESULT;
Status set_temporary_dir(CSlice dir) WARN_UNUSED_RESULT;
CSlice get_temporary_dir();
Result<std::pair<FileFd, string>> mkstemp(CSlice dir) WARN_UNUSED_RESULT;
Result<string> mkdtemp(CSlice dir, Slice prefix) WARN_UNUSED_RESULT;

template <class Func>
Status walk_path(CSlice path, Func &func) WARN_UNUSED_RESULT;

#ifdef TD_PORT_POSIX

// TODO move details somewhere else
namespace detail {
template <class Func>
Status walk_path_dir(string &path, FileFd fd, Func &&func) WARN_UNUSED_RESULT;
template <class Func>
Status walk_path_dir(string &path, Func &&func) WARN_UNUSED_RESULT;
template <class Func>
Status walk_path_file(string &path, Func &&func) WARN_UNUSED_RESULT;
template <class Func>
Status walk_path(string &path, Func &&func) WARN_UNUSED_RESULT;

template <class Func>
Status walk_path_subdir(string &path, DIR *dir, Func &&func) {
  while (true) {
    errno = 0;
    auto *entry = readdir(dir);
    auto readdir_errno = errno;
    if (readdir_errno) {
      return Status::PosixError(readdir_errno, "readdir");
    }
    if (entry == nullptr) {
      return Status::OK();
    }
    Slice name = Slice(&*entry->d_name);
    if (name == "." || name == "..") {
      continue;
    }
    auto size = path.size();
    if (path.back() != TD_DIR_SLASH) {
      path += TD_DIR_SLASH;
    }
    path.append(name.begin(), name.size());
    SCOPE_EXIT {
      path.resize(size);
    };
    Status status;
#ifdef DT_DIR
    if (entry->d_type == DT_UNKNOWN) {
      status = walk_path(path, std::forward<Func>(func));
    } else if (entry->d_type == DT_DIR) {
      status = walk_path_dir(path, std::forward<Func>(func));
    } else if (entry->d_type == DT_REG) {
      status = walk_path_file(path, std::forward<Func>(func));
    }
#else
#warning "Slow walk_path"
    status = walk_path(path, std::forward<Func>(func));
#endif
    if (status.is_error()) {
      return status;
    }
  }
}

template <class Func>
Status walk_path_dir(string &path, DIR *subdir, Func &&func) {
  SCOPE_EXIT {
    closedir(subdir);
  };
  TRY_STATUS(walk_path_subdir(path, subdir, std::forward<Func>(func)));
  std::forward<Func>(func)(path, true);
  return Status::OK();
}

template <class Func>
Status walk_path_dir(string &path, FileFd fd, Func &&func) {
  auto *subdir = fdopendir(fd.move_as_fd().move_as_native_fd());
  if (subdir == nullptr) {
    auto fdopendir_errno = errno;
    auto error = Status::PosixError(fdopendir_errno, "fdopendir");
    fd.close();
    return error;
  }
  return walk_path_dir(path, subdir, std::forward<Func>(func));
}

template <class Func>
Status walk_path_dir(string &path, Func &&func) {
  auto *subdir = opendir(path.c_str());
  if (subdir == nullptr) {
    auto opendir_errno = errno;
    return Status::PosixError(opendir_errno, PSTR() << tag("opendir", path));
  }
  return walk_path_dir(path, subdir, std::forward<Func>(func));
}

template <class Func>
Status walk_path_file(string &path, Func &&func) {
  std::forward<Func>(func)(path, false);
  return Status::OK();
}

template <class Func>
Status walk_path(string &path, Func &&func) {
  TRY_RESULT(fd, FileFd::open(path, FileFd::Read));
  auto stat = fd.stat();
  bool is_dir = stat.is_dir_;
  bool is_reg = stat.is_reg_;
  if (is_dir) {
    return walk_path_dir(path, std::move(fd), std::forward<Func>(func));
  }

  fd.close();
  if (is_reg) {
    return walk_path_file(path, std::forward<Func>(func));
  }

  return Status::OK();
}
}  // namespace detail

template <class Func>
Status walk_path(CSlice path, Func &&func) {
  string curr_path;
  curr_path.reserve(PATH_MAX + 10);
  curr_path = path.c_str();
  return detail::walk_path(curr_path, std::forward<Func>(func));
}

#endif  // TD_PORT_POSIX

#ifdef TD_PORT_WINDOWS

namespace detail {
template <class Func>
Status walk_path_dir(const wstring &dir_name, Func &&func) {
  wstring name = dir_name + L"\\*";

  WIN32_FIND_DATA file_data;
  auto handle = FindFirstFileExW(name.c_str(), FindExInfoStandard, &file_data, FindExSearchNameMatch, nullptr, 0);
  if (handle == INVALID_HANDLE_VALUE) {
    return Status::OsError(PSTR() << "FindFirstFileEx" << tag("name", to_string(name).ok()));
  }

  SCOPE_EXIT {
    FindClose(handle);
  };
  while (true) {
    auto full_name = dir_name + L"\\" + file_data.cFileName;
    TRY_RESULT(entry_name, to_string(full_name));
    if (file_data.cFileName[0] != '.') {
      if ((file_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        TRY_STATUS(walk_path_dir(full_name, func));
        func(entry_name, true);
      } else {
        func(entry_name, false);
      }
    }
    auto status = FindNextFileW(handle, &file_data);
    if (status == 0) {
      auto last_error = GetLastError();
      if (last_error == ERROR_NO_MORE_FILES) {
        return Status::OK();
      }
      return Status::OsError("FindNextFileW");
    }
  }
}
}  // namespace detail

template <class Func>
Status walk_path(CSlice path, Func &&func) {
  TRY_RESULT(wpath, to_wstring(path));
  return detail::walk_path_dir(wpath.c_str(), func);
}

#endif  // TD_PORT_WINDOWS

}  // namespace td
