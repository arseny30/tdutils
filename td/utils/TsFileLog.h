
#pragma once

#include "td/utils/common.h"
#include "td/utils/logging.h"
#include "td/utils/Status.h"

namespace td {
class TsFileLog {
  static constexpr int64 DEFAULT_ROTATE_THRESHOLD = 10 * (1 << 20);

 public:
  static Result<td::unique_ptr<LogInterface>> create(string path, int64 rotate_threshold = DEFAULT_ROTATE_THRESHOLD,
                                                     bool redirect_stderr = true);
};
}  // namespace td
