#include "td/utils/FileLog.h"

#include "td/utils/common.h"
#include "td/utils/logging.h"
#include "td/utils/port/FileFd.h"
#include "td/utils/port/path.h"
#include "td/utils/port/StdStreams.h"
#include "td/utils/Slice.h"

#include <limits>

namespace td {

Status FileLog::init(string path, int64 rotate_threshold) {
  if (path == path_) {
    set_rotate_threshold(rotate_threshold);
    return Status::OK();
  }

  TRY_RESULT(fd, FileFd::open(path, FileFd::Create | FileFd::Write | FileFd::Append));

  fd_.close();
  fd_ = std::move(fd);
  if (!Stderr().empty()) {
    fd_.get_native_fd().duplicate(Stderr().get_native_fd()).ignore();
  }

  path_ = std::move(path);
  size_ = fd_.get_size();
  rotate_threshold_ = rotate_threshold;
  return Status::OK();
}

Slice FileLog::get_path() const {
  return path_;
}

void FileLog::set_rotate_threshold(int64 rotate_threshold) {
  rotate_threshold_ = rotate_threshold;
}

int64 FileLog::get_rotate_threshold() const {
  return rotate_threshold_;
}

void FileLog::append(CSlice cslice, int log_level) {
  Slice slice = cslice;
  while (!slice.empty()) {
    auto r_size = fd_.write(slice);
    if (r_size.is_error()) {
      process_fatal_error(r_size.error().message());
    }
    auto written = r_size.ok();
    size_ += static_cast<int64>(written);
    slice.remove_prefix(written);
  }
  if (log_level == VERBOSITY_NAME(FATAL)) {
    process_fatal_error(cslice);
  }

  if (size_ > rotate_threshold_) {
    auto status = rename(path_, PSLICE() << path_ << ".old");
    if (status.is_error()) {
      process_fatal_error(status.message());
    }
    do_rotate();
  }
}

void FileLog::rotate() {
  if (path_.empty()) {
    return;
  }
  do_rotate();
}

void FileLog::do_rotate() {
  auto current_verbosity_level = GET_VERBOSITY_LEVEL();
  SET_VERBOSITY_LEVEL(std::numeric_limits<int>::min());  // to ensure that nothing will be printed to the closed log
  CHECK(!path_.empty());
  fd_.close();
  auto r_fd = FileFd::open(path_, FileFd::Create | FileFd::Truncate | FileFd::Write);
  if (r_fd.is_error()) {
    process_fatal_error(r_fd.error().message());
  }
  fd_ = r_fd.move_as_ok();
  if (!Stderr().empty()) {
    fd_.get_native_fd().duplicate(Stderr().get_native_fd()).ignore();
  }
  size_ = 0;
  SET_VERBOSITY_LEVEL(current_verbosity_level);
}

}  // namespace td
