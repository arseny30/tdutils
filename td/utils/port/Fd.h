#pragma once

#if 0
#include "td/utils/port/config.h"

#include "td/utils/common.h"
#include "td/utils/Status.h"

namespace td {

class Fd {
 public:
  static Status duplicate(const Fd &from, Fd &to);
};

}  // namespace td
#endif
