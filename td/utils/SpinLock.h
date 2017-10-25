#pragma once

#include <td/utils/port/thread.h>
#include <atomic>

namespace td {
namespace detail {
class InfBackoff {
 private:
  int cnt = 0;

 public:
  bool next() {
    cnt++;
    if (cnt < 50) {
      return true;
    } else {
      td::this_thread::yield();
      return true;
    }
  }
};
}  // namespace detail
class SpinLock {
 public:
  struct Unlock {
    void operator()(SpinLock *ptr) {
      ptr->unlock();
    }
  };
  using Lock = std::unique_ptr<SpinLock, Unlock>;

  Lock lock() {
    detail::InfBackoff backoff;
    while (!try_lock()) {
      backoff.next();
    }
    return Lock(this);
  }
  bool try_lock() {
    return !flag_.test_and_set(std::memory_order_acquire);
  }

 private:
  std::atomic_flag flag_;
  void unlock() {
    flag_.clear(std::memory_order_release);
  }
};
}  // namespace td
