#include "td/utils/StackAllocator.h"

namespace td {

StackAllocator::Impl &StackAllocator::impl() {
  static TD_THREAD_LOCAL StackAllocator::Impl *impl;  // static zero-initialized
  init_thread_local<Impl>(impl);
  return *impl;
}
}  // namespace td
