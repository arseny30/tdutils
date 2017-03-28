#pragma once
#include "td/utils/port/config.h"

#ifdef TD_THREAD_STL

#include <thread>

namespace td {
namespace detail {
class ThreadStl {
 public:
  ThreadStl() = default;
  ThreadStl(ThreadStl &&) = default;
  ThreadStl &operator=(ThreadStl &&) = default;
  template <class Function, class... Args>
  explicit ThreadStl(Function &&f, Args &&... args) {
    thread_ = std::thread([args = std::make_tuple(decay_copy(std::forward<Function>(f)),
                                                  decay_copy(std::forward<Args>(args))...)]() mutable {
      invoke_tuple(std::move(args));
      clear_thread_locals();
    });
  }
  void join() {
    thread_.join();
  }

 private:
  std::thread thread_;

  template <class T>
  std::decay_t<T> decay_copy(T &&v) {
    return std::forward<T>(v);
  }
};
}  // namespace detail
}  // namespace td

#endif  // TD_THREAD_STL
