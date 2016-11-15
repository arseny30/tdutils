#pragma once
#include "td/utils/port/config.h"

#ifdef TD_PORT_WINDOWS
#include "td/utils/Status.h"
namespace td {
class RwMutex {
 public:
  RwMutex() {
    init();
  }
  RwMutex(RwMutex &&other) {
    init();
    other.clear();
  }
  RwMutex &operator=(RwMutex &&other) {
    other.clear();
    return *this;
  }

  ~RwMutex() {
    clear();
  }

  bool empty() const {
    return !valid_;
  }
  void init() {
    CHECK(empty());
    valid_ = true;
    mutex_ = make_unique<SRWLOCK>();
    InitializeSRWLock(mutex_.get());
  }
  void clear() {
    if (valid_) {
      valid_ = false;
      mutex_.release();
    }
  }
  struct ReadUnlock {
    void operator()(RwMutex *ptr) {
      ptr->unlock_read();
    }
  };
  struct WriteUnlock {
    void operator()(RwMutex *ptr) {
      ptr->unlock_write();
    }
  };

  using ReadLock = std::unique_ptr<RwMutex, ReadUnlock>;
  using WriteLock = std::unique_ptr<RwMutex, WriteUnlock>;

  Result<ReadLock> lock_read() WARN_UNUSED_RESULT {
    lock_read_impl();
    return ReadLock(this);
  }
  Result<WriteLock> lock_write() WARN_UNUSED_RESULT {
    lock_write_impl();
    return WriteLock(this);
  }

 private:
  bool valid_ = false;
  unique_ptr<SRWLOCK> mutex_;

  void lock_read_impl() {
    CHECK(!empty());
    AcquireSRWLockShared(mutex_.get());
  }
  void lock_write_impl() {
    CHECK(!empty());
    AcquireSRWLockExclusive(mutex_.get());
  }
  void unlock_read() {
    CHECK(!empty());
    ReleaseSRWLockShared(mutex_.get());
  }
  void unlock_write() {
    CHECK(!empty());
    ReleaseSRWLockExclusive(mutex_.get());
  }
};
}  // namespace td
#endif  // TD_PORT_WINDOWS

#ifdef TD_PORT_POSIX

#include "td/utils/Status.h"

#include <pthread.h>

namespace td {
class RwMutex {
 public:
  RwMutex() {
    init();
  }
  RwMutex(RwMutex &&other) {
    init();
    other.clear();
  }
  RwMutex &operator=(RwMutex &&other) {
    other.clear();
    return *this;
  }
  ~RwMutex() {
    clear();
  }

  bool empty() const {
    return !valid_;
  }
  void init() {
    valid_ = true;
    pthread_rwlock_init(&mutex_, nullptr);
  }
  void clear() {
    if (valid_) {
      pthread_rwlock_destroy(&mutex_);
      valid_ = false;
    }
  }
  struct ReadUnlock {
    void operator()(RwMutex *ptr) {
      ptr->unlock_read();
    }
  };
  struct WriteUnlock {
    void operator()(RwMutex *ptr) {
      ptr->unlock_write();
    }
  };

  using ReadLock = std::unique_ptr<RwMutex, ReadUnlock>;
  using WriteLock = std::unique_ptr<RwMutex, WriteUnlock>;
  Result<ReadLock> lock_read() WARN_UNUSED_RESULT {
    lock_read_impl();
    return ReadLock(this);
  }
  Result<WriteLock> lock_write() WARN_UNUSED_RESULT {
    lock_write_impl();
    return WriteLock(this);
  }

 private:
  bool valid_;
  pthread_rwlock_t mutex_;

  void lock_read_impl() {
    // TODO error handling
    pthread_rwlock_rdlock(&mutex_);
  }
  void lock_write_impl() {
    pthread_rwlock_wrlock(&mutex_);
  }
  void unlock_read() {
    pthread_rwlock_unlock(&mutex_);
  }
  void unlock_write() {
    pthread_rwlock_unlock(&mutex_);
  }
};
}  // namespace td
#endif  // TD_PORT_POSIX
