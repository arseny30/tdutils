#pragma once

namespace td {

class Stacktrace {
 public:
  struct PrintOptions {
    bool use_gdb = false;
    PrintOptions() {
    }
  };
  static void print_to_stderr(const PrintOptions &options = PrintOptions());
  // backtrace needs to be called once to ensure that next calls are async-signal-safe
  static void init();
};

}  // namespace td
