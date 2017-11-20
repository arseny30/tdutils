#include "td/utils/tests.h"
#include "td/utils/HazardPointers.h"
#include "td/utils/port/thread.h"
#include "td/utils/Random.h"

TEST(HazardPointers, stress) {
  struct Node {
    std::atomic<std::string *> name_;
    char pad[64];
  };
  int threads_n = 10;
  std::vector<Node> nodes(threads_n);
  td::HazardPointers<std::string> hazard_pointers(threads_n);
  std::vector<td::thread> threads(threads_n);
  int id = 0;
  for (auto &thread : threads) {
    thread = td::thread([&, thread_id = id ] {
      for (int i = 0; i < 1000000; i++) {
        auto &node = nodes[td::Random::fast(0, threads_n - 1)];
        auto lock = hazard_pointers.protect(thread_id, 0, node.name_);
        auto *str = lock.get_ptr();
        if (str) {
          CHECK(*str == "one" || *str == "twotwo");
        }
        lock.reset();
        if (td::Random::fast(0, 5) == 0) {
          std::string *new_str = new std::string(td::Random::fast(0, 1) == 0 ? "one" : "twotwo");
          if (node.name_.compare_exchange_strong(str, new_str, std::memory_order_acq_rel)) {
            hazard_pointers.retire(thread_id, str);
          } else {
            delete new_str;
          }
        }
      }
    });
    id++;
  }
  for (auto &thread : threads) {
    thread.join();
  }
  LOG(ERROR) << "Undeleted pointers: " << hazard_pointers.to_delete_size_unsafe();
  CHECK(static_cast<int>(hazard_pointers.to_delete_size_unsafe()) < threads_n * threads_n);
}
