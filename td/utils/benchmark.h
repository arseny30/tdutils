#pragma once

#include "td/utils/port/Clocks.h"
#include "td/utils/format.h"
#include "td/utils/logging.h"

#include <cmath>
#include <tuple>

#define BENCH(name, desc)                      \
  class name##Bench : public ::td::Benchmark { \
   public:                                     \
    std::string get_description() override {   \
      return desc;                             \
    }                                          \
    void run(int n) override;                  \
  };                                           \
  void name##Bench::run(int n)

namespace td {
#if TD_MSVC

#pragma optimize("", off)
template <class T>
void do_not_optimize_away(T &&datum) {
  datum = datum;
}
#pragma optimize("", on)

#else
template <class T>
void do_not_optimize_away(T &&datum) {
  asm volatile("" : "+r"(datum));
}
#endif

class Benchmark {
 public:
  virtual ~Benchmark() = default;
  virtual std::string get_description() {
    return "";
  }

  virtual void start_up() {
  }
  virtual void start_up_n(int n) {
    start_up();
  }

  virtual void tear_down() {
  }

  virtual void run(int n) = 0;
};

inline std::pair<double, double> bench_n(Benchmark &&b, int n) {
  double total = -Clocks::monotonic();
  b.start_up_n(n);
  double t = -Clocks::monotonic();
  b.run(n);
  t += Clocks::monotonic();
  b.tear_down();
  total += Clocks::monotonic();

  return std::make_pair(t, total);
}

inline void bench(Benchmark &&b, double max_time = 1.0) {
  int n = 1;
  double pass_time = 0;
  double total_pass_time = 0;
  while (pass_time < max_time && total_pass_time < max_time * 3 && n < (1 << 30)) {
    n *= 2;
    std::tie(pass_time, total_pass_time) = bench_n(std::move(b), n);
  }
  pass_time = n / pass_time;

  int pass_cnt = 3;
  double sum = pass_time;
  double square_sum = pass_time * pass_time;
  double min_pass_time = pass_time;
  double max_pass_time = pass_time;

  for (int i = 1; i < pass_cnt; i++) {
    pass_time = n / bench_n(std::move(b), n).first;
    sum += pass_time;
    square_sum += pass_time * pass_time;
    if (pass_time < min_pass_time) {
      min_pass_time = pass_time;
    }
    if (pass_time > max_pass_time) {
      max_pass_time = pass_time;
    }
  }
  double avg = sum / pass_cnt;
  double d = sqrt(square_sum / pass_cnt - avg * avg);

  LOG(ERROR, "Bench [%40s]:\t%.3lf ops/sec,\t", b.get_description().c_str(), avg) << format::as_time(1 / avg)
                                                                                  << (PSTR(" [d = %.6lf]", d));
}
}  // namespace td
