#pragma once
#include "td/utils/port/config.h"
#include "td/utils/int_types.h"

#include <functional>

namespace td {
#ifdef TD_THREAD_PTHREAD
#define TD_THREAD_LOCAL __thread
#endif

#ifdef TD_THREAD_STL
#define TD_THREAD_LOCAL thread_local
#define TD_HAS_CPP_THREAD_LOCAL 1
#endif

extern TD_THREAD_LOCAL int32 thread_id_;
inline void set_thread_id(int32 id) {
  thread_id_ = id;
}
inline int32 get_thread_id() {
  return thread_id_;
}

// If raw_ptr is not nullptr, allocate T as in std::make_unique<T>(args...) and store pointer into raw_ptr
template <class T, class P, class... ArgsT>
bool init_thread_local(P& raw_ptr, ArgsT&&... args);
// Destroy all thread locals, and store nullptr into corresponding pointers
void clear_thread_locals();

namespace detail {
class Destructor {
 public:
  virtual ~Destructor() = default;
};
template <class F>
class LambdaDestructor : public Destructor {
 public:
  LambdaDestructor(F&& f) : f_(std::move(f)) {
  }
  ~LambdaDestructor() {
    f_();
  }

 private:
  F f_;
};

template <class F>
inline std::unique_ptr<Destructor> create_destructor(F&& f) {
  return std::make_unique<LambdaDestructor<F>>(std::move(f));
}

void add_thread_local_destructor(std::unique_ptr<Destructor> to_call);

template <class T, class P, class... ArgsT>
void do_init_thread_local(P& raw_ptr, ArgsT&&... args) {
  auto ptr = std::make_unique<T>(std::forward<ArgsT>(args)...);
  raw_ptr = ptr.get();

  detail::add_thread_local_destructor(detail::create_destructor([ ptr = std::move(ptr), &raw_ptr ]() mutable {
    ptr.reset();
    raw_ptr = 0;
  }));
}
}  // namespace detail

template <class T, class P, class... ArgsT>
inline bool init_thread_local(P& raw_ptr, ArgsT&&... args) {
  if (likely(raw_ptr != nullptr)) {
    return false;
  }
  detail::do_init_thread_local<T>(raw_ptr, std::forward<ArgsT>(args)...);
  return true;
}
}  // namespace td
