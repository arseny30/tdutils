#include "td/utils/tests.h"
#include "td/utils/OrderedEventsProcessor.h"
#include "td/utils/Random.h"

TEST(OrderedEventsProcessor, random) {
  int d = 5001;
  int n = 1000000;
  int offset = 1000000;
  std::vector<std::pair<int, int>> v;
  for (int i = 0; i < n; i++) {
    auto shift = td::Random::fast(0, 1) ? td::Random::fast(0, d) : td::Random::fast(0, 1) * d;
    v.push_back({i + shift, i + offset});
  }
  std::sort(v.begin(), v.end());

  td::OrderedEventsProcessor<int> processor(offset);
  int next = offset;
  for (auto p : v) {
    int seq_no = p.second;
    processor.add(seq_no, seq_no, [&](auto seq_no, int x) {
      CHECK(x == next);
      next++;
    });
  }
  CHECK(next == n + offset);
}
