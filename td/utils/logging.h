/***
* Simple logging.
* Interface is almost the same as in Google Logging library.
*
* FATAL, ERROR, WARNING, INFO, DEBUG
*
* LOG(WARNING) << "Hello world";
* LOG(INFO, "Hello %d", 1234) << "world";
* LOG_IF(INFO, condition) << "Hello world if condition";
*
* VLOG(x) <===> LOG_IF(DEBUG, verbosity_##x <= verbosity);
*
* LOG(FATAL, "power is off");
* CHECK(condition) <===> LOG_IF(FATAL, !(condition)
*
*
* - LOG_EVERY_N(INFO, 20) << "Hello world";
* - LOG_IF_EVERY_N(INFO, cond(), 20) << "Hello world";
*
* - LOG_FIRST_N(INFO, 20) << "Hello world";
* - LOG_IF_FIRST_N(INFO, cond(), 20) << "Hello world";
*/

#pragma once
#include "td/utils/port/Clocks.h"

#include "td/utils/common.h"

#include "td/utils/Slice-decl.h"
#include "td/utils/StringBuilder.h"
#include "td/utils/StackAllocator.h"

#include <cstdarg>  // for va_list, va_end, va_start
#include <cstdio>   // for snprintf, vsnprintf
#include <cstring>  // for memcpy, strrchr
#include <atomic>

#define PSTR(...) ::td::Slicify() & ::td::Logger(::td::NullLog().ref(), 0).printf(__VA_ARGS__)
#define LOG_PREFIX
#define LOG(level, ...) LOG_IF_INTERNAL(level, true, ::td::Slice(), __VA_ARGS__)
#define LOG_IF(level, condition, ...) LOG_IF_INTERNAL(level, condition, #condition, __VA_ARGS__)
#define VERBOSITY_NAME(x) verbosity_##x
#define STRIP_LOG VERBOSITY_NAME(DEBUG)
#define LOG_IS_NOT_STRIPPED(level) (VERBOSITY_NAME(level) <= STRIP_LOG)
#define LOG_IS_ON(level) (LOG_IS_NOT_STRIPPED(DEBUG) && VERBOSITY_NAME(level) <= ::td::verbosity_level)
#define LOG_IF_INTERNAL(level, condition, msg, ...) \
  LOG_IF_IMPL(level, LOG_IS_ON(level) && (condition), msg, __VA_ARGS__)
#define LOG_IF_IMPL(level, condition, msg, ...) \
  !(condition) ? (void)0 : ::td::Voidify() & LOGGER(level, msg).printf(__VA_ARGS__) LOG_PREFIX
#define VLOG_IS_ON_INNER(verbosity) (VERBOSITY_NAME(verbosity) <= ::td::verbosity_level)
#define VLOG_IS_ON(verbosity) LOG_IS_NOT_STRIPPED(DEBUG) && VLOG_IS_ON_INNER(verbosity)
#define VLOG(verbosity, ...) LOG_IF_INTERNAL(CUSTOM, VLOG_IS_ON_INNER(verbosity), #verbosity, __VA_ARGS__)
#define VLOG_IF(verbosity, cond, ...) \
  LOG_IF_INTERNAL(DEBUG, VLOG_IS_ON_INNER(verbosity) && (cond), #verbosity, __VA_ARGS__)
#define LOGGER(level, msg)                                                           \
  ::td::Logger(*::td::log_interface, VERBOSITY_NAME(level), __FILE__, __LINE__, msg, \
               VERBOSITY_NAME(level) == VERBOSITY_NAME(PLAIN))
#define LOG_ROTATE() ::td::log_interface->rotate()
#define LOG_TAG ::td::Logger::tag_
#define LOG_TAG2 ::td::Logger::tag2_

#define GET_VERBOSITY_LEVEL() (::td::verbosity_level)
#define SET_VERBOSITY_LEVEL(level) (::td::verbosity_level = level)

#ifdef __clang__
bool no_return_func() __attribute__((analyzer_noreturn));
#endif
inline bool no_return_func() {
  return true;
}

#ifdef CHECK
#undef CHECK
#endif
#ifdef TD_DEBUG
#if TD_MSVC
#define CHECK(condition, ...)       \
  __analysis_assume(!!(condition)); \
  LOG_IF(FATAL, !(condition), __VA_ARGS__)
#else
#define CHECK(condition, ...) LOG_IF(FATAL, !(condition) && no_return_func(), __VA_ARGS__)
#endif
#else
#define CHECK(condition, ...) LOG_IF(NEVER, !(condition), __VA_ARGS__)
#endif

#define UNREACHABLE(...)   \
  LOG(FATAL, __VA_ARGS__); \
  abort()

const int verbosity_CUSTOM = -6;
const int verbosity_PLAIN = -5;
const int verbosity_FATAL = -4;
const int verbosity_ERROR = -3;
const int verbosity_WARNING = -2;
const int verbosity_INFO = -1;
const int verbosity_DEBUG = 0;
const int verbosity_NEVER = 1024;
const int verbosity_0 = 0;
const int verbosity_1 = 1;
const int verbosity_2 = 2;
const int verbosity_3 = 3;
const int verbosity_4 = 4;
const int verbosity_5 = 5;
const int verbosity_6 = 6;
const int verbosity_7 = 7;
const int verbosity_8 = 8;
const int verbosity_9 = 9;
const int verbosity_10 = 10;
const int verbosity_11 = 11;

namespace td {
extern int verbosity_level;
// Not part of utils. Should be in some separate file
extern int verbosity_mtproto;
extern int verbosity_raw_mtproto;
extern int verbosity_dc;
extern int verbosity_fd;
extern int verbosity_net_query;
extern int verbosity_td_requests;
extern int verbosity_actor;
extern int verbosity_buffer;
extern int verbosity_files;
extern int verbosity_sqlite;

class LogInterface {
 public:
  virtual void append(const CSlice &slice, int log_level_) = 0;
  virtual void rotate() = 0;
};

class NullLog : public LogInterface {
 public:
  void append(const CSlice &slice, int log_level_) override {
  }
  void rotate() override {
  }
  NullLog &ref() {
    return *this;
  }
};

extern LogInterface *const default_log_interface;
extern LogInterface *log_interface;

#define TC_RED "\e[1;31m"
#define TC_BLUE "\e[1;34m"
#define TC_CYAN "\e[1;36m"
#define TC_GREEN "\e[1;32m"
#define TC_YELLOW "\e[1;33m"
#define TC_EMPTY "\e[0m"

class TsCerr {
 public:
  TsCerr();
  ~TsCerr();
  TsCerr &operator<<(Slice slice);

 private:
  typedef std::atomic_flag Lock;
  static Lock lock_;

  void enterCritical();
  void exitCritical();
};

class Logger {
 public:
  static const int BUFFER_SIZE = 128 * 1024;
  Logger(LogInterface &log, int log_level, bool simple_mode = false)
      : buffer_(StackAllocator<>::alloc(BUFFER_SIZE))
      , log_(log)
      , log_level_(log_level)
      , sb_(buffer_.as_slice())
      , simple_mode_(simple_mode) {
  }

  Logger(LogInterface &log, int log_level, Slice file_name, int line_num, Slice msg, bool simple_mode)
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

  template <class T>
  Logger &operator<<(const T &other) {
    sb_ << other;
    return *this;
  }

  Logger &printf() {
    return *this;
  }

  Logger &printf(const char *fmt, ...) IF_NO_MSVC(__attribute__((format(printf, 2 /*1 -- is this*/, 3)))) {
    if (!*fmt) {
      return *this;
    }
    va_list list;
    va_start(list, fmt);
    sb_.vprintf(fmt, list);
    va_end(list);
    return *this;
  }
  MutableCSlice as_cslice() {
    return sb_.as_cslice();
  }
  ~Logger() {
    if (!simple_mode_) {
      sb_ << '\n';
      auto slice = as_cslice();
      if (slice.back() != '\n') {
        slice.back() = '\n';
      }
    }

    log_.append(as_cslice(), log_level_);
  }

  StringBuilder &string_builder() {
    return sb_;
  }

  static TD_THREAD_LOCAL const char *tag_;
  static TD_THREAD_LOCAL const char *tag2_;

 private:
  StackAllocator<>::Ptr buffer_;
  LogInterface &log_;
  int log_level_;
  StringBuilder sb_;
  bool simple_mode_;
};

class Voidify {
 public:
  template <class T>
  void operator&(const T &) {
    return;
  }
};

class Slicify {
 public:
  CSlice operator&(Logger &logger) {
    return logger.as_cslice();
  }
};

class TsLog : public LogInterface {
 public:
  TsLog() : log_(nullptr) {
  }
  explicit TsLog(LogInterface *log) : log_(log) {
  }
  void init(LogInterface *log) {
    enter_critical();
    log_ = log;
    exit_critical();
  }
  void append(const CSlice &slice, int level) override {
    enter_critical();
    log_->append(slice, level);
    exit_critical();
  }
  void rotate() override {
    enter_critical();
    log_->rotate();
    exit_critical();
  }

 private:
  LogInterface *log_;
  std::atomic_flag lock_ = ATOMIC_FLAG_INIT;
  void enter_critical() {
    while (lock_.test_and_set(std::memory_order_acquire)) {
      // spin
    }
  }
  void exit_critical() {
    lock_.clear(std::memory_order_release);
  }
};
}  // end of namespace td

#include "td/utils/Slice.h"
