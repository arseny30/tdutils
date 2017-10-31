#pragma once
#include "td/utils/logging.h"

#include <atomic>
namespace td {
template <class T, int MaxPointersN = 1>
class HazardPointers {
 public:
  HazardPointers(size_t threads_n) : threads_(threads_n) {
    for (auto& data : threads_) {
      for (auto& ptr : data.hazard) {
        ptr = nullptr;
      }
    }
  }
  HazardPointers(const HazardPointers& other) = delete;
  HazardPointers(HazardPointers&& other) = delete;
  HazardPointers& operator=(const HazardPointers& other) = delete;
  HazardPointers& operator=(HazardPointers&& other) = delete;

  struct Lock {
    T* get_ptr() {
      return ptr.load(std::memory_order_relaxed);
    }
    std::atomic<T*>& ptr;
    bool owned{true};
    ~Lock() {
      reset();
    }
    void reset() {
      if (owned) {
        ptr.store(nullptr, std::memory_order_release);
        owned = false;
      }
    }
  };

  Lock protect(size_t thread_id, size_t pos, std::atomic<T*>& ptr) {
    CHECK(thread_id < threads_.size());
    CHECK(pos < MaxPointersN);
    auto& data = threads_[thread_id];
    auto& dest_ptr = data.hazard[pos];
    CHECK(!dest_ptr.load(std::memory_order_relaxed));

    T* saved = nullptr;
    while (true) {
      auto to_save = ptr.load();
      if (to_save == saved) {
        break;
      }
      dest_ptr.store(to_save);
      saved = to_save;
    }
    return Lock{dest_ptr};
  }

  void retire(size_t thread_id, T* ptr = nullptr) {
    CHECK(thread_id < threads_.size());
    auto& data = threads_[thread_id];
    if (ptr) {
      data.to_delete.push_back(std::unique_ptr<T>(ptr));
    }
    for (auto it = data.to_delete.begin(); it != data.to_delete.end();) {
      if (!is_hazard(it->get())) {
        it->reset();
        it = data.to_delete.erase(it);
      } else {
        ++it;
      }
    }
  }
  size_t to_delete_size_unsafe() const {
    size_t res = 0;
    for (auto& thread : threads_) {
      res += thread.to_delete.size();
    }
    return res;
  }

  bool is_hazard(T* ptr) {
    for (auto& thread : threads_) {
      for (auto& hazard_ptr : thread.hazard) {
        if (hazard_ptr.load() == ptr) {
          return true;
        }
      }
    }
    return false;
  }

 private:
  struct ThreadData {
    std::array<std::atomic<T*>, MaxPointersN> hazard;
    char pad[128];

    // stupid gc
    std::vector<std::unique_ptr<T>> to_delete;
    char pad2[128];
  };
  std::vector<ThreadData> threads_;
};
}  // namespace td
