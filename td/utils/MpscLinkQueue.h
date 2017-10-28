#pragma once
#include <atomic>
#include "td/utils/common.h"
namespace td {
//NB: holder of the queue holds all responsibility of freeing it's nodes
class MpscLinkQueueImpl {
 public:
  class Node;
  class Reader;

  void push(Node *node) {
    auto next = writer_node_.next_.load(std::memory_order_relaxed);
    do {
      node->next_.store(next, std::memory_order_relaxed);
    } while (
        !writer_node_.next_.compare_exchange_weak(next, node, std::memory_order_release, std::memory_order_relaxed));
  }

  void push_unsafe(Node *node) {
    node->next_.store(writer_node_.next_.load(std::memory_order_relaxed), std::memory_order_relaxed);
    writer_node_.next_.store(node, std::memory_order_relaxed);
  }

  Reader pop_all() {
    return Reader(writer_node_.next_.exchange(nullptr, std::memory_order_acquire));
  }

  Reader pop_all_unsafe() {
    return Reader(writer_node_.next_.exchange(nullptr, std::memory_order_relaxed));
  }

  class Node {
   private:
    friend class MpscLinkQueueImpl;
    std::atomic<Node *> next_{nullptr};
  };

  class Reader {
   public:
    Node *read() {
      auto old_head = head_;
      if (head_) {
        head_ = head_->next_.load(std::memory_order_relaxed);
      }
      return old_head;
    }

   private:
    friend class MpscLinkQueueImpl;
    Reader(Node *node) {
      // Reverse list
      Node *head = nullptr;
      while (node) {
        auto next = node->next_.load(std::memory_order_relaxed);
        node->next_.store(head, std::memory_order_relaxed);
        head = node;
        node = next;
      }
      head_ = head;
    }
    Node *head_;
  };

 private:
  Node writer_node_;
};

// Uses MpscLinkQueueImpl.
// Node should have to_mpsc_link_queue_node and from_mpsc_link_queue_node
// functions
template <class Node>
class MpscLinkQueue {
 public:
  void push(Node node) {
    impl_.push(node.to_mpsc_link_queue_node());
  }
  void push_unsafe(Node node) {
    impl_.push_unsafe(node.to_mpsc_link_queue_node());
  }
  class Reader {
   public:
    Reader(MpscLinkQueueImpl::Reader impl) : impl_(std::move(impl)) {
    }
    ~Reader() {
      CHECK(!pop());
    }
    Node pop() {
      auto node = impl_.read();
      if (!node) {
        return {};
      }
      return Node::from_mpsc_link_queue_node(node);
    }

   private:
    MpscLinkQueueImpl::Reader impl_;
  };

  Reader pop_all() {
    return Reader(impl_.pop_all());
  }
  Reader pop_all_unsafe() {
    return Reader(impl_.pop_all_unsafe());
  }

 private:
  MpscLinkQueueImpl impl_;
};

template <class Value>
class MpscLinkQueueUniquePtrNode {
 public:
  MpscLinkQueueUniquePtrNode() = default;
  MpscLinkQueueUniquePtrNode(std::unique_ptr<Value> ptr) : ptr_(std::move(ptr)) {
  }

  auto to_mpsc_link_queue_node() {
    return ptr_.release()->to_mpsc_link_queue_node();
  }
  static MpscLinkQueueUniquePtrNode<Value> from_mpsc_link_queue_node(td::MpscLinkQueueImpl::Node *node) {
    return MpscLinkQueueUniquePtrNode<Value>(std::unique_ptr<Value>(Value::from_mpsc_link_queue_node(node)));
  }

  operator bool() {
    return bool(ptr_);
  }

  Value &value() {
    return *ptr_;
  }

 private:
  std::unique_ptr<Value> ptr_;
};

}  // namespace td
