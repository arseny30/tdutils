#pragma once

#include "td/utils/common.h"
#include "td/utils/logging.h"
#include "td/utils/port/Fd.h"
#include "td/utils/port/FileFd.h"
#include "td/utils/port/path.h"
#include "td/utils/Slice.h"

#include <cstdlib>
#include <limits>

namespace td {

// TODO move implementation to cpp
class FileLog : public LogInterface {
  static constexpr int DEFAULT_ROTATE_THRESHOLD = 10 * (1 << 20);

 public:
  void append(CSlice xslice, int log_level) override {
    Slice slice = xslice;
    while (!slice.empty()) {
      auto r_size = fd_.write(slice);
      if (r_size.is_error()) {
        std::abort();
      }
      auto written = r_size.ok();
      size_ += static_cast<int64>(written);
      slice.remove_prefix(written);
    }
    if (log_level == VERBOSITY_NAME(FATAL)) {
      std::abort();
    }

    if (size_ > rotate_threshold_) {
      auto status = rename(path_, path_ + ".old");
      if (status.is_error()) {
        std::abort();
      }
      do_rotate();
    }
  }

  void rotate() override {
    if (path_.empty()) {
      return;
    }
    do_rotate();
  }

  void init(string path, int64 rotate_threshold = DEFAULT_ROTATE_THRESHOLD) {
    path_ = std::move(path);

    auto r_fd = FileFd::open(path_, FileFd::Create | FileFd::Write | FileFd::Append);
    LOG_IF(FATAL, r_fd.is_error()) << "Can't open log: " << r_fd.error();
    fd_ = r_fd.move_as_ok();
    Fd::duplicate(fd_.get_fd(), Fd::Stderr()).ignore();
    size_ = fd_.get_size();
    rotate_threshold_ = rotate_threshold;
  }

 private:
  FileFd fd_;
  string path_;
  int64 size_;
  int64 rotate_threshold_;

  void do_rotate() {
    auto current_verbosity_level = GET_VERBOSITY_LEVEL();
    SET_VERBOSITY_LEVEL(std::numeric_limits<int>::min());  // to ensure that nothing will be printed to the closed log
    CHECK(!path_.empty());
    fd_.close();
    auto r_fd = FileFd::open(path_, FileFd::Create | FileFd::Truncate | FileFd::Write);
    if (r_fd.is_error()) {
      std::abort();
    }
    fd_ = r_fd.move_as_ok();
    Fd::duplicate(fd_.get_fd(), Fd::Stderr()).ignore();
    size_ = 0;
    SET_VERBOSITY_LEVEL(current_verbosity_level);
  }
};
}  // namespace td
