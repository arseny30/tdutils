#pragma once

#include "td/utils/common.h"
#include "td/utils/port/thread_local.h"

#include <array>
#include <atomic>

namespace td {

class ThreadSafeCounter {
 public:
  void add(int64 diff) {
    auto &node = thread_local_node();
    node.count_.store(node.count_.load(std::memory_order_relaxed) + diff, std::memory_order_relaxed);
  }

  int64 sum() const {
    int n = max_thread_id_.load();
    int64 res = 0;
    for (int i = 0; i < n; i++) {
      res += nodes_[i].count_.load();
    }
    return res;
  }

 private:
  struct Node {
    std::atomic<int64> count_{0};
    char padding[128];
  };
  static constexpr int MAX_THREAD_ID = 128;
  std::atomic<int> max_thread_id_{MAX_THREAD_ID};
  std::array<Node, MAX_THREAD_ID> nodes_;

  Node &thread_local_node() {
    auto thread_id = get_thread_id();
    CHECK(static_cast<size_t>(thread_id) < nodes_.size());
    return nodes_[thread_id];
  }
};

}  // namespace td
