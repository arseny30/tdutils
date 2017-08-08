#pragma once

#include "td/utils/port/config.h"

#include "td/utils/common.h"
#include "td/utils/port/Stat.h"
#include "td/utils/Slice.h"
#include "td/utils/Status.h"

#ifdef TD_PORT_POSIX

#include <atomic>

namespace td {

class ObserverBase;

template <class F>
auto skip_eintr(F &&f) {
  decltype(f()) res;
  do {
    res = f();
  } while (res < 0 && errno == EINTR);
  return res;
}

class Fd {
 public:
  // TODO: Close may be not enough
  // Sometimes descriptor is half closed.

  enum Flag : int32 {
    Write = 0x001,
    Read = 0x002,
    Close = 0x004,
    Error = 0x008,
    All = Write | Read | Close | Error,
    None = 0
  };
  using Flags = int32;
  enum class Mode { Reference, Own };

  Fd();
  Fd(const Fd &) = delete;
  Fd &operator=(const Fd &) = delete;
  Fd(Fd &&other);
  Fd &operator=(Fd &&other);
  ~Fd();

  Fd(int fd, Mode mode);

  Fd clone();
  static Fd &Stderr();
  static Fd &Stdout();
  static Fd &Stdin();
  static Status duplicate(const Fd &from, Fd &to);

  bool empty() const;
  bool is_ref() const {
    return mode_ == Mode::Reference;
  }

  const Fd &get_fd() const;
  Fd &get_fd();

  int get_native_fd() const;
  int move_as_native_fd();

  Status set_is_blocking(bool is_blocking);

  void set_observer(ObserverBase *observer);
  ObserverBase *get_observer() const;

  void close();

  void update_flags_notify(Flags flags);
  void update_flags(Flags flags);
  void clear_flags(Flags flags);

  bool can_read() const;
  bool can_write() const;
  bool can_close() const;
  int32 get_flags() const;

  bool has_pending_error() const;
  Status get_pending_error() WARN_UNUSED_RESULT;

  Result<size_t> write(Slice slice) WARN_UNUSED_RESULT;
  Result<size_t> write_unsafe(Slice slice) WARN_UNUSED_RESULT;
  Result<size_t> read(MutableSlice slice) WARN_UNUSED_RESULT;

  Stat stat() const;

 private:
  struct Info {
    std::atomic<int> refcnt;
    int32 flags;
    ObserverBase *observer;
  };
  struct InfoSet {
    InfoSet();
    Info &get_info(int32 id);

   private:
    static constexpr int MAX_FD = 65536;
    Info fd_array_[MAX_FD];
  };
  static InfoSet fd_info_set_;

  int fd_ = -1;
  Mode mode_ = Mode::Own;

  static Fd stderr_;
  static Fd stdout_;
  static Fd stdin_;

  void update_flags_inner(int32 new_flags, bool notify_flag);
  Info *get_info();
  const Info *get_info() const;
  void clear_info();

  void close_ref();
  void close_own();
};

template <class FdT>
bool can_read(const FdT &fd) {
  return fd.get_flags() & Fd::Read;
}

template <class FdT>
bool can_write(const FdT &fd) {
  return fd.get_flags() & Fd::Write;
}

template <class FdT>
bool can_close(const FdT &fd) {
  return fd.get_flags() & Fd::Close;
}

}  // namespace td
#endif  // TD_PORT_POSIX

#ifdef TD_PORT_WINDOWS

#include "td/utils/port/IPAddress.h"

namespace td {

class ObserverBase;

namespace detail {
class EventFdWindows;
class FdImpl;
}  // namespace detail
// like linux fd.
// also it handles fd destruction
// descriptor(handler) itself should be create by some external function

class Fd {
 public:
  enum Flag : int32 {
    Write = 0x001,
    Read = 0x002,
    Close = 0x004,
    Error = 0x008,
    All = Write | Read | Close | Error,
    None = 0
  };
  using Flags = int32;
  Fd() = default;

  const Fd &get_fd() const;
  Fd &get_fd();

  Result<size_t> write(Slice slice) WARN_UNUSED_RESULT;
  bool empty() const;
  void close();
  Result<size_t> read(MutableSlice slice) WARN_UNUSED_RESULT;
  Result<size_t> pwrite(Slice slice, off_t pos) WARN_UNUSED_RESULT;
  Result<size_t> pread(MutableSlice slice, off_t pos) WARN_UNUSED_RESULT;

  Result<Fd> accept() WARN_UNUSED_RESULT;
  void connect(const IPAddress &addr);

  Stat stat() const;
  off_t get_size() const;
  Status sync() WARN_UNUSED_RESULT;

  Fd clone() const;
  uint64 get_key() const;

  void set_observer(ObserverBase *observer);
  ObserverBase *get_observer() const;

  Flags get_flags() const;
  void update_flags(Flags flags);
  bool can_read() const;
  bool can_write() const;
  bool can_close() const;

  bool has_pending_error() const;
  Status get_pending_error() WARN_UNUSED_RESULT;

  HANDLE get_read_event();
  HANDLE get_write_event();
  void on_read_event();
  void on_write_event();

  static Fd &Stderr();
  static Fd &Stdin();
  static Fd &Stdout();
  static Status duplicate(const Fd &from, Fd &to);

 private:
  enum class Type {
    Empty,
    EventFd,
    FileFd,
    StdinFileFd,
    SocketFd,
    ServerSocketFd,
  };
  enum class Mode { Owner, Reference };

  friend class FileFd;
  friend class SocketFd;
  friend class ServerSocketFd;
  friend class detail::EventFdWindows;
  friend class detail::FdImpl;

  Mode mode_ = Mode::Owner;
  shared_ptr<detail::FdImpl> impl_;

  Fd(Type type, Mode mode, HANDLE handle);
  Fd(Type type, Mode mode, SOCKET sock, int32 socket_family = 0);
  explicit Fd(shared_ptr<detail::FdImpl> impl);

  void acquire();
  void release();
};

inline bool can_read(const Fd &fd) {
  return (fd.get_flags() & Fd::Read) != 0;
}

inline bool can_write(const Fd &fd) {
  return (fd.get_flags() & Fd::Write) != 0;
}

inline bool can_close(const Fd &fd) {
  return (fd.get_flags() & Fd::Close) != 0;
}

}  // namespace td
#endif  // TD_PORT_WINDOWS
