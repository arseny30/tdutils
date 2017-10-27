#pragma once
#include "td/utils/port/config.h"

#ifdef TD_THREAD_PTHREAD

#include "td/utils/common.h"
#include "td/utils/invoke.h"
#include "td/utils/MovableValue.h"

#include <tuple>
#include <type_traits>
#include <utility>

#include <pthread.h>
#include <sched.h>

namespace td {
namespace detail {
class ThreadPthread {
 public:
  ThreadPthread() = default;
  ThreadPthread(const ThreadPthread &other) = delete;
  ThreadPthread &operator=(const ThreadPthread &other) = delete;
  ThreadPthread(ThreadPthread &&) = default;
  ThreadPthread &operator=(ThreadPthread &&) = default;
  template <class Function, class... Args>
  explicit ThreadPthread(Function &&f, Args &&... args) {
    func_ = std::make_unique<std::unique_ptr<Destructor>>(
        create_destructor([args = std::make_tuple(decay_copy(std::forward<Function>(f)),
                                                  decay_copy(std::forward<Args>(args))...)]() mutable {
          invoke_tuple(std::move(args));
          clear_thread_locals();
        }));
    pthread_create(&thread_, nullptr, run_thread, func_.get());
    is_inited_ = true;
  }
  void join() {
    if (is_inited_.get()) {
      is_inited_ = false;
      pthread_join(thread_, nullptr);
    }
  }
  ~ThreadPthread() {
    join();
  }
  using id = pthread_t;

 private:
  MovableValue<bool> is_inited_;
  pthread_t thread_;
  std::unique_ptr<std::unique_ptr<Destructor>> func_;

  template <class T>
  std::decay_t<T> decay_copy(T &&v) {
    return std::forward<T>(v);
  }

  static void *run_thread(void *ptr) {
    auto func = static_cast<decltype(func_.get())>(ptr);
    func->reset();
    return nullptr;
  }
};

namespace this_thread_pthread {
inline void yield() {
  sched_yield();
}
inline ThreadPthread::id get_id() {
  return pthread_self();
}
}  // namespace this_thread_pthread
}  // namespace detail
}  // namespace td

#endif
