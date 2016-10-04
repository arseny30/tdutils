#pragma once
#include "td/utils/common.h"

#include <functional>

template <class FunctionT = std::function<void(void)>>
class ScopeGuard {
 public:
  ScopeGuard(const FunctionT &func) : func_(func) {
  }
  ScopeGuard(FunctionT &&func) : func_(std::move(func)) {
  }
  ScopeGuard(ScopeGuard &&other) : dismissed_(other.dismissed_), func_(std::move(other.func_)) {
    other.dismissed_ = true;
  }

  void dismiss() {
    dismissed_ = true;
  }

  ~ScopeGuard() {
    if (!dismissed_) {
      func_();
    }
  }

 private:
  bool dismissed_ = false;
  FunctionT func_;
};

enum class ScopeExit {};
template <class FunctionT>
auto operator+(ScopeExit, FunctionT &&func) {
  return ScopeGuard<std::decay_t<FunctionT>>(std::forward<FunctionT>(func));
}

#define SCOPE_EXIT auto TD_ANONYMOUS_VARIABLE(SCOPE_EXIT_VAR) = ScopeExit() + [&]()
