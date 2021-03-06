#pragma once

#include "td/utils/common.h"
#include "td/utils/port/thread.h"

#include <atomic>
#include <cstring>
#include <memory>
#include <type_traits>

namespace td {

template <class T>
class AtomicRead {
 public:
  void read(T &dest) const {
    while (true) {
      static_assert(std::is_trivially_copyable<T>::value, "T must be trivially copyable");
      auto version_before = version.load();
      if (version_before % 2 == 0) {
        std::memcpy(&dest, &value, sizeof(dest));
        auto version_after = version.load();
        if (version_before == version_after) {
          break;
        }
      }
      td::this_thread::yield();
    }
  }

  struct Write {
    explicit Write(AtomicRead *read) {
      read->do_lock();
      ptr.reset(read);
    }
    struct Destructor {
      void operator()(AtomicRead *read) const {
        read->do_unlock();
      }
    };
    T &operator*() {
      return value();
    }
    T *operator->() {
      return &value();
    }
    T &value() {
      CHECK(ptr);
      return ptr->value;
    }

   private:
    std::unique_ptr<AtomicRead, Destructor> ptr;
  };

  Write lock() {
    return Write(this);
  }

 private:
  std::atomic<uint64> version{0};
  T value;

  void do_lock() {
    bool is_locked = ++version % 2 == 1;
    CHECK(is_locked);
  }
  void do_unlock() {
    bool is_unlocked = ++version % 2 == 0;
    CHECK(is_unlocked);
  }
};

}  // namespace td
