#include "td/utils/common.h"
#include "td/utils/FileLog.h"

namespace td {
TD_THREAD_LOCAL int32 thread_id_ = -1;

static FileLog file_log;
static TsLog ts_log(&file_log);
void tdlib_init(string path) {
  SET_VERBOSITY_LEVEL(verbosity_WARNING);
  chdir(path).ensure();
  file_log.init("log.txt", std::numeric_limits<off_t>::max());
  log_interface = &file_log;
}
}
