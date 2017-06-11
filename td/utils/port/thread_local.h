#pragma once

#include "td/utils/port/config.h"

#include "td/utils/int_types.h"

#include <memory>

namespace td {

// clang-format off
#if TD_GCC || TD_CLANG
  #define TD_THREAD_LOCAL __thread
#else
  #define TD_THREAD_LOCAL thread_local
#endif
// clang-format on

// If raw_ptr is not nullptr, allocate T as in std::make_unique<T>(args...) and store pointer into raw_ptr
template <class T, class P, class... ArgsT>
bool init_thread_local(P &raw_ptr, ArgsT &&... args);

// Destroy all thread locals, and store nullptr into corresponding pointers
void clear_thread_locals();

void set_thread_id(int32 id);

int32 get_thread_id();

namespace detail {
extern TD_THREAD_LOCAL int32 thread_id_;

class Destructor {
 public:
  Destructor() = default;
  Destructor(const Destructor &other) = delete;
  Destructor &operator=(const Destructor &other) = delete;
  Destructor(Destructor &&other) = default;
  Destructor &operator=(Destructor &&other) = default;
  virtual ~Destructor() = default;
};

template <class F>
class LambdaDestructor : public Destructor {
 public:
  LambdaDestructor(F &&f) : f_(std::move(f)) {
  }
  LambdaDestructor(const LambdaDestructor &other) = delete;
  LambdaDestructor &operator=(const LambdaDestructor &other) = delete;
  LambdaDestructor(LambdaDestructor &&other) = default;
  LambdaDestructor &operator=(LambdaDestructor &&other) = default;
  ~LambdaDestructor() override {
    f_();
  }

 private:
  F f_;
};

template <class F>
std::unique_ptr<Destructor> create_destructor(F &&f) {
  return std::make_unique<LambdaDestructor<F>>(std::forward<F>(f));
}

void add_thread_local_destructor(std::unique_ptr<Destructor> destructor);

template <class T, class P, class... ArgsT>
void do_init_thread_local(P &raw_ptr, ArgsT &&... args) {
  auto ptr = std::make_unique<T>(std::forward<ArgsT>(args)...);
  raw_ptr = ptr.get();

  detail::add_thread_local_destructor(detail::create_destructor([ ptr = std::move(ptr), &raw_ptr ]() mutable {
    ptr.reset();
    raw_ptr = nullptr;
  }));
}
}  // namespace detail

template <class T, class P, class... ArgsT>
bool init_thread_local(P &raw_ptr, ArgsT &&... args) {
  if (likely(raw_ptr != nullptr)) {
    return false;
  }
  detail::do_init_thread_local<T>(raw_ptr, std::forward<ArgsT>(args)...);
  return true;
}

inline void set_thread_id(int32 id) {
  detail::thread_id_ = id;
}

inline int32 get_thread_id() {
  return detail::thread_id_;
}

}  // namespace td
