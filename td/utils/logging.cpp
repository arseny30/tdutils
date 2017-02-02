#include "td/utils/logging.h"

#include "td/utils/FileLog.h"
#include "td/utils/misc.h"

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <atomic>

#include "td/utils/port/Fd.h"
#include "td/utils/Time.h"

#if TD_ANDROID
#include <android/log.h>
#define ALOG_TAG "DLTD"
#endif

namespace td {
int verbosity_level = 1;
int verbosity_net_query = 1;
int verbosity_td_requests = 1;
int verbosity_dc = 2;
int verbosity_files = 2;
int verbosity_mtproto = 7;
int verbosity_raw_mtproto = 8;
int verbosity_fd = 9;
int verbosity_actor = 10;
int verbosity_buffer = 10;
int verbosity_sqlite = 10;

TD_THREAD_LOCAL const char *Logger::tag_ = nullptr;
TD_THREAD_LOCAL const char *Logger::tag2_ = nullptr;

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
      // Resource temporary unavalible.
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
  void append(const CSlice &slice, int log_level) override {
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
      case VERBOSITY_NAME(CUSTOM):
      case VERBOSITY_NAME(DEBUG):
        __android_log_write(ANDROID_LOG_DEBUG, ALOG_TAG, slice.c_str());
        break;
    }
#endif

#ifdef TD_PORT_POSIX
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

}  // end of namespace td
