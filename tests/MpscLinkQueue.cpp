#include "td/utils/tests.h"
#include "td/utils/MpscLinkQueue.h"
#include "td/utils/format.h"
#include "td/utils/port/thread.h"

class NodeX : public td::MpscLinkQueueImpl::Node {
 public:
  NodeX(int value) : value_(value) {
  }
  auto to_mpsc_link_queue_node() {
    return static_cast<td::MpscLinkQueueImpl::Node *>(this);
  }
  static NodeX *from_mpsc_link_queue_node(td::MpscLinkQueueImpl::Node *node) {
    return static_cast<NodeX *>(node);
  }
  int value() {
    return value_;
  }

 private:
  int value_;
};
using QueueNode = td::MpscLinkQueueUniquePtrNode<NodeX>;

QueueNode create_node(int value) {
  return std::make_unique<NodeX>(value);
}

TEST(MpscLinkQueue, one_thread) {
  td::MpscLinkQueue<QueueNode> queue;

  {
    queue.push(create_node(1));
    queue.push(create_node(2));
    queue.push(create_node(3));
    td::MpscLinkQueue<QueueNode>::Reader reader;
    queue.pop_all(reader);
    queue.push(create_node(4));
    queue.pop_all(reader);
    std::vector<int> v;
    while (auto node = reader.read()) {
      v.push_back(node.value().value());
    }
    CHECK((v == std::vector<int>{1, 2, 3, 4})) << td::format::as_array(v);

    v.clear();
    queue.push(create_node(5));
    queue.pop_all(reader);
    while (auto node = reader.read()) {
      v.push_back(node.value().value());
    }
    CHECK((v == std::vector<int>{5})) << td::format::as_array(v);
  }

  {
    queue.push_unsafe(create_node(3));
    queue.push_unsafe(create_node(2));
    queue.push_unsafe(create_node(1));
    queue.push_unsafe(create_node(0));
    td::MpscLinkQueue<QueueNode>::Reader reader;
    queue.pop_all_unsafe(reader);
    std::vector<int> v;
    while (auto node = reader.read()) {
      v.push_back(node.value().value());
    }
    CHECK((v == std::vector<int>{3, 2, 1, 0})) << td::format::as_array(v);
  }
}

TEST(MpscLinkQueue, multi_thread) {
  td::MpscLinkQueue<QueueNode> queue;
  int threads_n = 10;
  int queries_n = 1000000;
  std::vector<int> next(threads_n);
  std::vector<td::thread> threads(threads_n);
  int thread_i = 0;
  for (auto &thread : threads) {
    thread = td::thread([&, id = thread_i ] {
      for (int i = 0; i < queries_n; i++) {
        queue.push(create_node(i * threads_n + id));
      }
    });
    thread_i++;
  }

  int active_threads = threads_n;

  td::MpscLinkQueue<QueueNode>::Reader reader;
  while (active_threads) {
    queue.pop_all(reader);
    while (auto value = reader.read()) {
      auto x = value.value().value();
      auto thread_id = x % threads_n;
      x /= threads_n;
      CHECK(next[thread_id] == x);
      next[thread_id]++;
      if (x + 1 == queries_n) {
        active_threads--;
      }
    }
  }

  for (auto &thread : threads) {
    thread.join();
  }
}
