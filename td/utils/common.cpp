#include "td/utils/common.h"
#include "td/utils/FileLog.h"

namespace td {
TD_THREAD_LOCAL int32 thread_id_ = -1;
}
