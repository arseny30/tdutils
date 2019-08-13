#pragma once

#include "td/utils/common.h"
#include "td/utils/FileLog.h"
#include "td/utils/logging.h"
#include "td/utils/Status.h"

namespace td {

class TsFileLog {
 public:
  static Result<unique_ptr<LogInterface>> create(string path);
};

}  // namespace td
