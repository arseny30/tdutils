#pragma once

#include "td/utils/common.h"
#include "td/utils/logging.h"
#include "td/utils/ScopeGuard.h"
#include "td/utils/Slice.h"
#include "td/utils/StackAllocator.h"
#include "td/utils/StringBuilder.h"

#include <cerrno>
#include <cstring>  // for strerror
#include <new>

#if TD_PORT_WINDOWS
#include <codecvt>
#include <locale>
#endif

#define TRY_STATUS(status)               \
  {                                      \
    auto try_status = (status);          \
    if (try_status.is_error()) {         \
      return try_status.move_as_error(); \
    }                                    \
  }
#define TRY_RESULT(name, result) TRY_RESULT_IMPL(TD_CONCAT(r_, name), name, result)

#define TRY_RESULT_IMPL(r_name, name, result) \
  auto r_name = (result);                     \
  if (r_name.is_error()) {                    \
    return r_name.move_as_error();            \
  }                                           \
  auto name = r_name.move_as_ok();

#define LOG_STATUS(status)                      \
  {                                             \
    auto log_status = (status);                 \
    if (log_status.is_error()) {                \
      LOG(ERROR) << log_status.move_as_error(); \
    }                                           \
  }

#if TD_PORT_WINDOWS
#define OS_ERROR(message) [&](){ auto saved_error = ::GetLastError(); return ::td::Status::WindowsError(saved_error, (message)); }()
#define OS_SOCKET_ERROR(message) [&](){ auto saved_error = ::WSAGetLastError(); return ::td::Status::WindowsError(saved_error, (message)); }()
#endif
#if TD_PORT_POSIX
#define OS_ERROR(message) [&](){ auto saved_errno = errno; return ::td::Status::PosixError(saved_errno, (message)); }()
#define OS_SOCKET_ERROR(message) OS_ERROR(message)
#endif

namespace td {

#if TD_PORT_POSIX
CSlice strerror_safe(int code);
#endif
#if TD_PORT_WINDOWS
string winerror_to_string(int code);
#endif

class Status {
 private:
  enum class ErrorType : int8 { general, os };

 public:
  Status() = default;

  bool operator==(const Status &other) const {
    if (get_info().static_flag) {
      return ptr_ == other.ptr_;
    }
    return false;
  }

  Status clone() const WARN_UNUSED_RESULT {
    if (is_ok()) {
      return Status();
    }
    auto info = get_info();
    if (info.static_flag) {
      return clone_static();
    }
    return Status(false, info.error_type, info.error_code, message());
  }

  static Status OK() WARN_UNUSED_RESULT {
    return Status();
  }

  static Status Error(int err, Slice message = Slice()) WARN_UNUSED_RESULT {
    return Status(false, ErrorType::general, err, message);
  }

  static Status Error(Slice message) WARN_UNUSED_RESULT {
    return Error(0, message);
  }

#if TD_PORT_WINDOWS
  static Status WindowsError(int error_code, Slice message) WARN_UNUSED_RESULT {
    return Status(false, ErrorType::os, error_code, message);
  }
#endif

#if TD_PORT_POSIX
  static Status PosixError(int32 errno_code, Slice message) WARN_UNUSED_RESULT {
    return Status(false, ErrorType::os, errno_code, message);
  }
#endif

  static Status Error() WARN_UNUSED_RESULT {
    return Error<0>();
  }

  template <int Code>
  static Status Error() {
    static Status status(true, ErrorType::general, Code, "");
    return status.clone_static();
  }

  template <int Code>
  static Status PosixError() {
    static Status status(true, ErrorType::os, Code, "");
    return status.clone_static();
  }

  static Status InvalidId() WARN_UNUSED_RESULT {
    static Status status(true, ErrorType::general, 0, "Invalid Id");
    return status.clone_static();
  }
  static Status Hangup() WARN_UNUSED_RESULT {
    static Status status(true, ErrorType::general, 0, "Hangup");
    return status.clone_static();
  }

  StringBuilder &print(StringBuilder &sb) const {
    if (is_ok()) {
      return sb << "OK";
    }
    Info info = get_info();
    switch (info.error_type) {
      case ErrorType::general:
        sb << "[Error";
        break;
      case ErrorType::os:
#if TD_PORT_POSIX
        sb << "[PosixError : " << strerror_safe(info.error_code);
#endif
#if TD_PORT_WINDOWS
        sb << "[WindowsError : " << winerror_to_string(info.error_code);
#endif
        break;
      default:
        LOG(FATAL) << "Unknown status type: " << static_cast<int8>(info.error_type);
        UNREACHABLE();
        break;
    }
    sb << " : " << code() << " : " << message() << "]";
    return sb;
  }

  string to_string() const {
    auto buf = StackAllocator::alloc(4096);
    StringBuilder sb(buf.as_slice());
    print(sb);
    return sb.as_cslice().str();
  }

  // Default interface
  bool is_ok() const WARN_UNUSED_RESULT {
    return !is_error();
  }

  bool is_error() const WARN_UNUSED_RESULT {
    return ptr_ != nullptr;
  }

  void ensure() const {
    if (!is_ok()) {
      LOG(FATAL) << "FAILED: " << to_string();
    }
  }
  void ensure_error() const {
    if (is_ok()) {
      LOG(FATAL) << "Expected Status::Error";
    }
  }
  Status &log_ensure() {
    if (!is_ok()) {
      LOG(ERROR) << "FAILED: " << to_string();
    }
    return *this;
  }

  void ignore() const {
    // nop
  }

  int32 code() const {
    if (is_ok()) {
      return 0;
    }
    return get_info().error_code;
  }

  CSlice message() const {
    if (is_ok()) {
      return CSlice("OK");
    }
    return CSlice(ptr_.get() + sizeof(Info));
  }

  string public_message() const {
    if (is_ok()) {
      return "OK";
    }
    Info info = get_info();
    switch (info.error_type) {
      case ErrorType::general:
        return message().str();
      case ErrorType::os:
#if TD_PORT_POSIX
        return strerror_safe(info.error_code).str();
#endif
#if TD_PORT_WINDOWS
        return winerror_to_string(info.error_code);
#endif
      default:
        LOG(FATAL) << "Unknown status type: " << static_cast<int8>(info.error_type);
        UNREACHABLE();
        return "";
    }
  }

  const Status &error() const {
    return *this;
  }

  Status move() WARN_UNUSED_RESULT {
    return std::move(*this);
  }

  Status move_as_error() WARN_UNUSED_RESULT {
    return std::move(*this);
  }

 private:
  struct Info {
    bool static_flag : 1;
    signed int error_code : 23;
    ErrorType error_type;
  };

  struct Deleter {
    void operator()(char *ptr) {
      if (!get_info(ptr).static_flag) {
        delete[] ptr;
      }
    }
  };
  std::unique_ptr<char[], Deleter> ptr_;

  Status(Info info, Slice message) {
    size_t size = sizeof(Info) + message.size() + 1;
    ptr_ = std::unique_ptr<char[], Deleter>(new char[size]);
    char *ptr = ptr_.get();
    reinterpret_cast<Info *>(ptr)[0] = info;
    ptr += sizeof(Info);
    std::memcpy(ptr, message.begin(), message.size());
    ptr += message.size();
    *ptr = 0;
  }

  Status(bool static_flag, ErrorType error_type, int error_code, Slice message)
      : Status(to_info(static_flag, error_type, error_code), message) {
  }

  Status clone_static() const WARN_UNUSED_RESULT {
    CHECK(is_ok() || get_info().static_flag);
    Status result;
    result.ptr_ = std::unique_ptr<char[], Deleter>(ptr_.get());
    return result;
  }

  static Info to_info(bool static_flag, ErrorType error_type, int error_code) {
    const int MIN_ERROR_CODE = -(1 << 22) + 1;
    const int MAX_ERROR_CODE = (1 << 22) - 1;
    Info tmp;
    tmp.static_flag = static_flag;
    tmp.error_type = error_type;

    if (error_code < MIN_ERROR_CODE) {
      LOG(ERROR) << "Error code value is altered from " << error_code;
      error_code = MIN_ERROR_CODE;
    }
    if (error_code > MAX_ERROR_CODE) {
      LOG(ERROR) << "Error code value is altered from " << error_code;
      error_code = MAX_ERROR_CODE;
    }

#if TD_GCC
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#endif
    tmp.error_code = error_code;
#if TD_GCC
#pragma GCC diagnostic pop
#endif
    CHECK(error_code == tmp.error_code);
    return tmp;
  }

  Info get_info() const {
    return get_info(ptr_.get());
  }
  static Info get_info(char *ptr) {
    return reinterpret_cast<Info *>(ptr)[0];
  }
};

template <class T = Unit>
class Result {
 public:
  Result() : status_(Status::Error()) {
  }
  template <class S>
  Result(S &&x) : status_(), value_(std::forward<S>(x)) {
  }
  Result(Status &&status) : status_(std::move(status)) {
    CHECK(status_.is_error());
  }
  Result(const Result &) = delete;
  Result &operator=(const Result &) = delete;
  Result(Result &&other) : status_(std::move(other.status_)) {
    if (status_.is_ok()) {
      new (&value_) T(std::move(other.value_));
      other.value_.~T();
    }
    other.status_ = Status::Error();
  }
  Result &operator=(Result &&other) {
    if (status_.is_ok()) {
      value_.~T();
    }
    if (other.status_.is_ok()) {
#if TD_GCC
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
      new (&value_) T(std::move(other.value_));
#if TD_GCC
#pragma GCC diagnostic pop
#endif
      other.value_.~T();
    }
    status_ = std::move(other.status_);
    other.status_ = Status::Error();
    return *this;
  }
  ~Result() {
    if (status_.is_ok()) {
      value_.~T();
    }
  }

  void ensure() const {
    status_.ensure();
  }
  void ensure_error() const {
    status_.ensure_error();
  }
  Result &log_ensure() {
    status_.log_ensure();
    return *this;
  }
  void ignore() const {
    status_.ignore();
  }
  bool is_ok() const {
    return status_.is_ok();
  }
  bool is_error() const {
    return status_.is_error();
  }
  const Status &error() const {
    CHECK(status_.is_error());
    return status_;
  }
  Status move_as_error() WARN_UNUSED_RESULT {
    CHECK(status_.is_error());
    SCOPE_EXIT {
      status_ = Status::Error();
    };
    return std::move(status_);
  }
  const T &ok() const {
    CHECK(status_.is_ok()) << status_;
    return value_;
  }
  T &ok_ref() {
    CHECK(status_.is_ok()) << status_;
    return value_;
  }
  T move_as_ok() {
    CHECK(status_.is_ok()) << status_;
    return std::move(value_);
  }

  Result<T> clone() const WARN_UNUSED_RESULT {
    if (is_ok()) {
      return Result<T>(ok());  // TODO: return clone(ok());
    }
    return error().clone();
  }
  void clear() {
    *this = Result<T>();
  }

 private:
  Status status_;
  union {
    T value_;
  };
};

template <>
inline Result<Unit>::Result(Status &&status) : status_(std::move(status)) {
  // no assert
}

inline StringBuilder &operator<<(StringBuilder &string_builder, const Status &status) {
  return status.print(string_builder);
}

// TODO move to_string somewhere else

#if TD_PORT_WINDOWS
template <class Facet>
class usable_facet : public Facet {
 public:
  template <class... Args>
  explicit usable_facet(Args &&... args) : Facet(std::forward<Args>(args)...) {
  }
  ~usable_facet() = default;
};

inline Result<wstring> to_wstring(Slice slice) {
  // TODO(perf): optimize
  std::wstring_convert<usable_facet<std::codecvt_utf8_utf16<wchar_t>>> converter;
  auto res = converter.from_bytes(slice.begin(), slice.end());
  if (converter.converted() != slice.size()) {
    return Status::Error("Wrong encoding");
  }
  return res;
}

inline Result<string> to_string(const wchar_t *begin, size_t size) {
  std::wstring_convert<usable_facet<std::codecvt_utf8_utf16<wchar_t>>> converter;
  auto res = converter.to_bytes(begin, begin + size);
  if (converter.converted() != size) {
    return Status::Error("Wrong encoding");
  }
  return res;
}

inline Result<string> to_string(const wstring &str) {
  return to_string(str.data(), str.size());
}
inline Result<string> to_string(const wchar_t *begin) {
  return to_string(begin, wcslen(begin));
}

inline string winerror_to_string(int code) {
  const size_t size = 1000;
  wchar_t wbuf[size];
  auto res_size = ::FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,                 // It's a system error
                                  nullptr,                                    // No string to be formatted needed
                                  code,                                       // Hey Windows: Please explain this error!
                                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),  // Do it in the standard language
                                  wbuf,                                       // Put the message here
                                  size - 1,                                   // Number of bytes to store the message
                                  nullptr);
  if (res_size == 0) {
    return string("Unknown windows error");
  }
  while (res_size != 0 && (wbuf[res_size - 1] == '\n' || wbuf[res_size - 1] == '\r')) {
    res_size--;
  }
  return to_string(wbuf, res_size).ok();
}
#endif

namespace detail {
class SlicifySafe {
 public:
  Result<CSlice> operator&(Logger &logger) {
    if (logger.is_error()) {
      return Status::Error("Buffer overflow");
    }
    return logger.as_cslice();
  }
};
class StringifySafe {
 public:
  Result<string> operator&(Logger &logger) {
    if (logger.is_error()) {
      return Status::Error("Buffer overflow");
    }
    return logger.as_cslice().str();
  }
};
}  // namespace detail
}  // namespace td
