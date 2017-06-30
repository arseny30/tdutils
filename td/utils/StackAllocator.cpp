#include "td/utils/StackAllocator.h"

namespace td {
TD_THREAD_LOCAL StackAllocator::Impl *StackAllocator::impl_;  // static zero initialized

StackAllocator::Impl &StackAllocator::impl() {
  init_thread_local<Impl>(impl_);
  return *impl_;
}
}  // namespace td
