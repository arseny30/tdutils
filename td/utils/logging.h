#pragma once

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
* VLOG(x) <===> LOG_IF(DEBUG, VERBOSITY_NAME(x) <= verbosity);
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

#include "td/utils/common.h"
#include "td/utils/Slice-decl.h"
#include "td/utils/StackAllocator.h"
#include "td/utils/StringBuilder.h"

#include <atomic>
#include <cstdlib>

#define PSTR_IMPL(...) ::td::Logger(::td::NullLog().ref(), 0).printf(__VA_ARGS__)
#define PSLICE(...) ::td::detail::Slicify() & PSTR_IMPL(__VA_ARGS__)
#define PSTRING(...) ::td::detail::Stringify() & PSTR_IMPL(__VA_ARGS__)
#define PSLICE_SAFE(...) ::td::detail::SlicifySafe() & PSTR_IMPL(__VA_ARGS__)
#define PSTRING_SAFE(...) ::td::detail::StringifySafe() & PSTR_IMPL(__VA_ARGS__)
#define LOG_PREFIX
#define LOG(level, ...) LOG_IF_INTERNAL(level, true, ::td::Slice(), __VA_ARGS__)
#define LOG_IF(level, condition, ...) LOG_IF_INTERNAL(level, condition, #condition, __VA_ARGS__)
#define VERBOSITY_NAME(x) verbosity_##x
#define STRIP_LOG VERBOSITY_NAME(DEBUG)
#define LOG_IS_NOT_STRIPPED(level) (VERBOSITY_NAME(level) <= STRIP_LOG)
#define LOG_IS_ON(level) (LOG_IS_NOT_STRIPPED(DEBUG) && VERBOSITY_NAME(level) <= GET_VERBOSITY_LEVEL())
#define LOG_IF_INTERNAL(level, condition, msg, ...) \
  LOG_IF_IMPL(level, LOG_IS_ON(level) && (condition), msg, __VA_ARGS__)
#define LOG_IF_IMPL(level, condition, msg, ...) \
  !(condition) ? (void)0 : ::td::Voidify() & LOGGER(level, msg).printf(__VA_ARGS__) LOG_PREFIX
#define VLOG_IS_ON_INNER(verbosity) (VERBOSITY_NAME(verbosity) <= GET_VERBOSITY_LEVEL())
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

#define GET_VERBOSITY_LEVEL() (::td::VERBOSITY_NAME(level))
#define SET_VERBOSITY_LEVEL(new_level) (::td::VERBOSITY_NAME(level) = (new_level))

#if TD_CLANG
bool no_return_func() __attribute__((analyzer_noreturn));
#endif
inline bool no_return_func() {
  return true;
}

// clang-format off
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
// clang-format on

#define UNREACHABLE(...)   \
  LOG(FATAL, __VA_ARGS__); \
  std::abort()

constexpr int VERBOSITY_NAME(CUSTOM) = -6;
constexpr int VERBOSITY_NAME(PLAIN) = -5;
constexpr int VERBOSITY_NAME(FATAL) = -4;
constexpr int VERBOSITY_NAME(ERROR) = -3;
constexpr int VERBOSITY_NAME(WARNING) = -2;
constexpr int VERBOSITY_NAME(INFO) = -1;
constexpr int VERBOSITY_NAME(DEBUG) = 0;
constexpr int VERBOSITY_NAME(NEVER) = 1024;

namespace td {
extern int VERBOSITY_NAME(level);
// TODO Not part of utils. Should be in some separate file
extern int VERBOSITY_NAME(mtproto);
extern int VERBOSITY_NAME(raw_mtproto);
extern int VERBOSITY_NAME(connections);
extern int VERBOSITY_NAME(dc);
extern int VERBOSITY_NAME(fd);
extern int VERBOSITY_NAME(net_query);
extern int VERBOSITY_NAME(td_requests);
extern int VERBOSITY_NAME(actor);
extern int VERBOSITY_NAME(buffer);
extern int VERBOSITY_NAME(files);
extern int VERBOSITY_NAME(sqlite);

class LogInterface {
 public:
  LogInterface() = default;
  LogInterface(const LogInterface &) = delete;
  LogInterface &operator=(const LogInterface &) = delete;
  LogInterface(LogInterface &&) = delete;
  LogInterface &operator=(LogInterface &&) = delete;
  virtual ~LogInterface() = default;
  virtual void append(CSlice slice, int log_level_) = 0;
  virtual void rotate() = 0;
};

class NullLog : public LogInterface {
 public:
  void append(CSlice slice, int log_level_) override {
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
  TsCerr(const TsCerr &) = delete;
  TsCerr &operator=(const TsCerr &) = delete;
  TsCerr(TsCerr &&) = delete;
  TsCerr &operator=(TsCerr &&) = delete;
  ~TsCerr();
  TsCerr &operator<<(Slice slice);

 private:
  using Lock = std::atomic_flag;
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

  Logger(LogInterface &log, int log_level, Slice file_name, int line_num, Slice msg, bool simple_mode);

  template <class T>
  Logger &operator<<(const T &other) {
    sb_ << other;
    return *this;
  }

  Logger &printf() {
    return *this;
  }

  Logger &printf(const char *fmt, ...) ATTRIBUTE_FORMAT_PRINTF(2, 3);

  MutableCSlice as_cslice() {
    return sb_.as_cslice();
  }
  bool is_error() const {
    return sb_.is_error();
  }
  Logger(const Logger &) = delete;
  Logger &operator=(const Logger &) = delete;
  Logger(Logger &&) = delete;
  Logger &operator=(Logger &&) = delete;
  ~Logger();

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
  }
};

namespace detail {
class Slicify {
 public:
  CSlice operator&(Logger &logger) {
    return logger.as_cslice();
  }
};
class Stringify {
 public:
  string operator&(Logger &logger) {
    return logger.as_cslice().str();
  }
};
}  // namespace detail

class TsLog : public LogInterface {
 public:
  TsLog() = default;
  explicit TsLog(LogInterface *log) : log_(log) {
  }
  void init(LogInterface *log) {
    enter_critical();
    log_ = log;
    exit_critical();
  }
  void append(CSlice slice, int level) override {
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
  LogInterface *log_ = nullptr;
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
}  // namespace td

#include "td/utils/Slice.h"
