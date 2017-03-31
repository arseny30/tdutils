#include "td/utils/logging.h"

#include "td/utils/FileLog.h"
#include "td/utils/misc.h"
#include "td/utils/port/Clocks.h"
#include "td/utils/port/Fd.h"
#include "td/utils/Time.h"

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <iostream>

#if TD_ANDROID
#include <android/log.h>
#define ALOG_TAG "DLTD"
#elif TD_TIZEN
#include <dlog.h>
#define DLOG_TAG "DLTD"
#endif

namespace td {
int verbosity_level = 1;
int verbosity_net_query = 1;
int verbosity_td_requests = 1;
int verbosity_dc = 2;
int verbosity_files = 2;
int verbosity_mtproto = 7;
int verbosity_connections = 8;
int verbosity_raw_mtproto = 10;
int verbosity_fd = 9;
int verbosity_actor = 10;
int verbosity_buffer = 10;
int verbosity_sqlite = 10;

TD_THREAD_LOCAL const char *Logger::tag_ = nullptr;
TD_THREAD_LOCAL const char *Logger::tag2_ = nullptr;

Logger::Logger(LogInterface &log, int log_level, Slice file_name, int line_num, Slice msg, bool simple_mode)
    : Logger(log, log_level, simple_mode) {
  if (simple_mode) {
    return;
  }

  auto last_slash_ = static_cast<int32>(file_name.size()) - 1;
  while (last_slash_ >= 0 && file_name[last_slash_] != '/' && file_name[last_slash_] != '\\') {
    last_slash_--;
  }
  file_name = file_name.substr(last_slash_ + 1);

  printf("[%2d]", log_level);
  auto tid = get_thread_id();
  if (tid != -1) {
    printf("[t%2d]", tid);
  }
  printf("[%.9lf]", Clocks::system());
  (*this) << "[" << file_name << ":" << line_num << "]";
  if (tag_ != nullptr && *tag_) {
    (*this) << "[#" << Slice(tag_) << "]";
  }
  if (tag2_ != nullptr && *tag2_) {
    (*this) << "[!" << Slice(tag2_) << "]";
  }
  if (!msg.empty()) {
    (*this) << "[&" << msg << "]";
  }
  (*this) << "\t";
}

Logger &Logger::printf(const char *fmt, ...) {
  if (!*fmt) {
    return *this;
  }
  va_list list;
  va_start(list, fmt);
  sb_.vprintf(fmt, list);
  va_end(list);
  return *this;
}

Logger::~Logger() {
  if (!simple_mode_) {
    sb_ << '\n';
    auto slice = as_cslice();
    if (slice.back() != '\n') {
      slice.back() = '\n';
    }
  }

  log_.append(as_cslice(), log_level_);
}

TsCerr::TsCerr() {
  enterCritical();
}
TsCerr::~TsCerr() {
  exitCritical();
}
TsCerr &TsCerr::operator<<(Slice slice) {
  double end_time = 0;
  while (!slice.empty()) {
    auto res = Fd::Stderr().write(slice);
    if (res.is_error()) {
      if (res.error().code() == EPIPE) {
        break;
      }
      // Resource temporary unavailable
      if (end_time == 0) {
        end_time = Time::now() + 0.01;
      } else if (Time::now() > end_time) {
        break;
      }
      continue;
    }
    slice.remove_prefix(res.ok());
  }
  return *this;
}

void TsCerr::enterCritical() {
  while (lock_.test_and_set(std::memory_order_acquire)) {
    // spin
  }
}

void TsCerr::exitCritical() {
  lock_.clear(std::memory_order_release);
}
TsCerr::Lock TsCerr::lock_ = ATOMIC_FLAG_INIT;

class DefaultLog : public LogInterface {
 public:
  void append(CSlice slice, int log_level) override {
#if TD_ANDROID
    switch (log_level) {
      case VERBOSITY_NAME(FATAL):
        __android_log_write(ANDROID_LOG_FATAL, ALOG_TAG, slice.c_str());
        break;
      case VERBOSITY_NAME(ERROR):
        __android_log_write(ANDROID_LOG_ERROR, ALOG_TAG, slice.c_str());
        break;
      case VERBOSITY_NAME(WARNING):
        __android_log_write(ANDROID_LOG_WARN, ALOG_TAG, slice.c_str());
        break;
      case VERBOSITY_NAME(INFO):
        __android_log_write(ANDROID_LOG_INFO, ALOG_TAG, slice.c_str());
        break;
      case VERBOSITY_NAME(DEBUG):
      case VERBOSITY_NAME(CUSTOM):
      default:
        __android_log_write(ANDROID_LOG_DEBUG, ALOG_TAG, slice.c_str());
        break;
    }
#elif TD_TIZEN
    switch (log_level) {
      case VERBOSITY_NAME(FATAL):
        dlog_print(DLOG_ERROR, DLOG_TAG, slice.c_str());
        break;
      case VERBOSITY_NAME(ERROR):
        dlog_print(DLOG_ERROR, DLOG_TAG, slice.c_str());
        break;
      case VERBOSITY_NAME(WARNING):
        dlog_print(DLOG_WARN, DLOG_TAG, slice.c_str());
        break;
      case VERBOSITY_NAME(INFO):
        dlog_print(DLOG_INFO, DLOG_TAG, slice.c_str());
        break;
      case VERBOSITY_NAME(DEBUG):
      case VERBOSITY_NAME(CUSTOM):
      default:
        dlog_print(DLOG_DEBUG, DLOG_TAG, slice.c_str());
        break;
    }
#elif TD_PORT_POSIX
    Slice color;
    switch (log_level) {
      case VERBOSITY_NAME(FATAL):
      case VERBOSITY_NAME(ERROR):
        color = TC_RED;
        break;
      case VERBOSITY_NAME(WARNING):
        color = TC_YELLOW;
        break;
      case VERBOSITY_NAME(INFO):
        color = TC_CYAN;
        break;
    }
    TsCerr() << color << slice << TC_EMPTY;
#else
    // TODO: color
    TsCerr() << slice;
#endif
    if (log_level == VERBOSITY_NAME(FATAL)) {
      abort();
    }
  }
  void rotate() override {
  }

 private:
  // TODO MemoryLog
};
static DefaultLog default_log;

LogInterface *const default_log_interface = &default_log;
LogInterface *log_interface = default_log_interface;

}  // namespace td
