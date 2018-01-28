#pragma once

#include "td/utils/common.h"
#include "td/utils/logging.h"
#include "td/utils/port/FileFd.h"
#include "td/utils/Slice.h"

namespace td {

class FileLog : public LogInterface {
  static constexpr int64 DEFAULT_ROTATE_THRESHOLD = 10 * (1 << 20);

 public:
  bool init(string path, int64 rotate_threshold = DEFAULT_ROTATE_THRESHOLD);

  void set_rotate_threshold(int64 rotate_threshold);

  void append(CSlice cslice, int log_level) override;

  void rotate() override;

 private:
  FileFd fd_;
  string path_;
  int64 size_;
  int64 rotate_threshold_;

  void do_rotate();
};

}  // namespace td
