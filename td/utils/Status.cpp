#include "td/utils/Status.h"

namespace td {
TD_THREAD_LOCAL char *strerror_safe_buf;  // static zero initialized
}  // namespace td
