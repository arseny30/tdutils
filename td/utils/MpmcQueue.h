#pragma once
// MPMC queue
// Simple semaphore protected implementation
// To close queue, one shoud send as musch sentinel elements as there are readers.
// Once there are no readers and writers, one may easily destroy queue

#include <atomic>
#include <mutex>

#include "td/utils/logging.h"
#include "td/utils/HazardPointers.h"
#include "td/utils/port/thread.h"

namespace td {
template <class T>
class OneValue {
 public:
  bool set_value(T &value) {
    value_ = std::move(value);
    auto old_state = state_.load(std::memory_order_relaxed);
    while (true) {
      Type new_state;
      switch (old_state) {
        case Empty:
          new_state = Value;
          break;
        case Taken:
          value = std::move(value_);
          return false;
        case Value:
          return true;
      }
      state_.compare_exchange_strong(old_state, new_state, std::memory_order_acq_rel);
    }
  }
  bool get_value(T &value) {
    auto old_state = state_.load(std::memory_order_acquire);
    while (true) {
      Type new_state;
      switch (old_state) {
        case Empty:
          new_state = Taken;
          break;
        case Taken:
          return false;
        case Value:
          value = std::move(value_);
          return true;
      }
      state_.compare_exchange_strong(old_state, new_state, std::memory_order_acq_rel);
    }
  }
  void reset() {
    state_ = Empty;
    value_ = T();
  }

 private:
  enum Type : int { Empty = 0, Taken, Value };
  std::atomic<int> state_{Empty};
  T value_;
};

template <class T>
class MpmcQueueBlock {
 public:
  MpmcQueueBlock(size_t size) : nodes_(size) {
  }
  enum class PopStatus { Ok, Empty, Closed };

  //blocking pop
  //returns Ok or Closed
  PopStatus pop(T &value) {
    while (true) {
      auto read_pos = read_pos_.load(std::memory_order_relaxed);
      if (read_pos >= nodes_.size()) {
        return PopStatus::Closed;
      }
      read_pos = read_pos_.fetch_add(1, std::memory_order_release);
      if (read_pos >= nodes_.size()) {
        return PopStatus::Closed;
      }
      //TODO blocking get_value
      if (nodes_[read_pos].one_value.get_value(value)) {
        return PopStatus::Ok;
      }
    }
  }

  //nonblocking pop
  //returns Ok, Empty or Closed
  PopStatus try_pop(T &value) {
    while (true) {
      auto read_pos = read_pos_.load(std::memory_order_relaxed);
      auto write_pos = write_pos_.load(std::memory_order_relaxed);
      if (read_pos >= nodes_.size()) {
        return PopStatus::Closed;
      }
      if (write_pos <= read_pos) {
        return PopStatus::Empty;
      }
      read_pos = read_pos_.fetch_add(1, std::memory_order_release);
      if (read_pos >= nodes_.size()) {
        return PopStatus::Closed;
      }
      if (nodes_[read_pos].one_value.get_value(value)) {
        return PopStatus::Ok;
      }
    }
  }

  enum class PushStatus { Ok, Closed };
  PushStatus push(T &value) {
    while (true) {
      auto write_pos = write_pos_.load(std::memory_order_relaxed);
      if (write_pos >= nodes_.size()) {
        return PushStatus::Closed;
      }
      write_pos = write_pos_.fetch_add(1, std::memory_order_release);
      if (write_pos >= nodes_.size()) {
        return PushStatus::Closed;
      }
      if (nodes_[write_pos].one_value.set_value(value)) {
        return PushStatus::Ok;
      }
    }
  }

 private:
  struct Node {
    OneValue<T> one_value;
    char pad[64];
  };
  std::vector<Node> nodes_;
  char pad[64];
  std::atomic<uint32> write_pos_{0};
  char pad2[64];
  std::atomic<uint32> read_pos_{0};
};

template <class T>
class MpmcQueue {
 public:
  class Backoff {
   private:
    int cnt = 0;

   public:
    bool next() {
      cnt++;
      if (cnt < 1) {  // 50
        return true;
      } else {
        td::this_thread::yield();
        return cnt < 3;  // 500
      }
    }
  };
  MpmcQueue(size_t block_size, size_t threads_n) : block_size_{block_size}, hazard_pointers_{threads_n} {
    auto node = std::make_unique<Node>(block_size_);
    write_pos_ = node.get();
    read_pos_ = node.get();
    node.release();
  }

  size_t hazard_pointers_to_delele_size_unsafe() const {
    return hazard_pointers_.to_delete_size_unsafe();
  }
  void gc(size_t thread_id) {
    hazard_pointers_.retire(thread_id);
  }

  using PushStatus = typename MpmcQueueBlock<T>::PushStatus;
  using PopStatus = typename MpmcQueueBlock<T>::PopStatus;
  void push(T value, size_t thread_id) {
    bool loop_flag{true};
    while (loop_flag) {
      auto lock = hazard_pointers_.protect(thread_id, 0, write_pos_);
      auto node = lock.get_ptr();
      auto status = node->block.push(value);
      switch (status) {
        case PushStatus::Ok:
          return;
        case PushStatus::Closed: {
          auto next = node->next_.load(std::memory_order_acquire);
          if (next == nullptr) {
            auto new_node = new Node(block_size_);
            new_node->block.push(value);
            if (node->next_.compare_exchange_strong(next, new_node, std::memory_order_acq_rel)) {
              next = new_node;
              loop_flag = false;
            } else {
              auto status = new_node->block.pop(value);
              CHECK(status == PopStatus::Ok);
              delete new_node;
            }
          }
          if (next != nullptr) {
            write_pos_.compare_exchange_strong(node, next, std::memory_order_acq_rel);
          }
          break;
        }
      }
    }
  }

  T pop(size_t thread_id) {
    Backoff backoff;
    while (true) {
      auto lock = hazard_pointers_.protect(thread_id, 0, read_pos_);
      auto node = lock.get_ptr();
      T value;
      auto status = node->block.pop(value);
      switch (status) {
        case PopStatus::Ok:
          return value;
        case PopStatus::Empty:
          UNREACHABLE();
        case PopStatus::Closed: {
          auto next = node->next_.load(std::memory_order_acquire);
          if (next == nullptr) {
            backoff.next();
            next = node->next_.load(std::memory_order_acquire);
          }
          if (next != nullptr) {
            if (read_pos_.compare_exchange_strong(node, next, std::memory_order_acq_rel)) {
              hazard_pointers_.retire(thread_id, node);
            }
          }
          break;
        }
      }
    }
  }

 private:
  struct Node {
    Node(size_t block_size) : block{block_size} {
    }
    MpmcQueueBlock<T> block;
    std::atomic<Node *> next_{nullptr};
  };
  size_t block_size_;
  HazardPointers<Node, 1> hazard_pointers_;
  std::atomic<Node *> write_pos_;
  std::atomic<Node *> read_pos_;
};

}  // namespace td
