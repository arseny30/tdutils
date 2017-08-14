#include "td/utils/port/signals.h"

#ifdef TD_PORT_POSIX
#include <signal.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace td {

#ifdef TD_PORT_POSIX
static Status protect_memory(void *addr, size_t len) {
  if (mprotect(addr, len, PROT_NONE) != 0) {
    auto mprotect_errno = errno;
    return Status::PosixError(mprotect_errno, "mprotect failed");
  }
  return Status::OK();
}
#endif

Status setup_signals_alt_stack() {
#ifdef TD_PORT_POSIX
  auto page_size = getpagesize();
  auto stack_size = (MINSIGSTKSZ + 16 * page_size - 1) / page_size * page_size;

  void *stack = mmap(nullptr, stack_size + 2 * page_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
  if (stack == MAP_FAILED) {
    auto mmap_errno = errno;
    return Status::PosixError(mmap_errno, "Mmap failed");
  }

  TRY_STATUS(protect_memory(stack, page_size));
  TRY_STATUS(protect_memory(static_cast<char *>(stack) + stack_size + page_size, page_size));

  stack_t signal_stack;
  signal_stack.ss_sp = static_cast<char *>(stack) + page_size;
  signal_stack.ss_size = stack_size;
  signal_stack.ss_flags = 0;

  if (sigaltstack(&signal_stack, nullptr) < 0) {
    auto sigaltstack_errno = errno;
    return Status::PosixError(sigaltstack_errno, "sigaltstack failed");
  }
#endif
  return Status::OK();
}

}  // namespace td
