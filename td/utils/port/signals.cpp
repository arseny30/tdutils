#include "td/utils/port/signals.h"

#ifdef TD_PORT_POSIX
#include <signal.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

#include <cerrno>
#include <cstring>
#include <ctime>
#include <limits>

#include "td/utils/format.h"
#include "td/utils/logging.h"

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

  if (sigaltstack(&signal_stack, nullptr) != 0) {
    auto sigaltstack_errno = errno;
    return Status::PosixError(sigaltstack_errno, "sigaltstack failed");
  }
#endif
  return Status::OK();
}

#ifdef TD_PORT_POSIX
template <class F>
static Status set_signal_handler_impl(vector<int> signals, F func, bool is_extended = false) {
  struct sigaction act;
  std::memset(&act, '\0', sizeof(act));
  if (is_extended) {  // TODO if constexpr, remove useless reinterpret_cast
    act.sa_handler = reinterpret_cast<decltype(act.sa_handler)>(func);
  } else {
    act.sa_sigaction = reinterpret_cast<decltype(act.sa_sigaction)>(func);
  }
  sigemptyset(&act.sa_mask);
  for (auto signal : signals) {
    sigaddset(&act.sa_mask, signal);
  }
  act.sa_flags = SA_RESTART | SA_ONSTACK;
  if (is_extended) {
    act.sa_flags |= SA_SIGINFO;
  }

  for (auto signal : signals) {
    if (sigaction(signal, &act, nullptr) != 0) {
      auto sigaction_errno = errno;
      return Status::PosixError(sigaction_errno, "sigaction failed");
    }
  }
  return Status::OK();
}

static vector<int> get_native_signals(SignalType type) {
  switch (type) {
    case SignalType::Abort:
      return {SIGABRT, SIGXCPU, SIGXFSZ};
    case SignalType::Error:
      return {SIGILL, SIGFPE, SIGBUS, SIGSEGV, SIGSYS};
    case SignalType::Quit:
      return {SIGINT, SIGTERM, SIGQUIT};
    case SignalType::Pipe:
      return {SIGPIPE};
    case SignalType::HangUp:
      return {SIGHUP};
    case SignalType::User:
      return {SIGUSR1, SIGUSR2};
    case SignalType::Other:
      return {SIGTRAP, SIGALRM, SIGVTALRM, SIGPROF, SIGTSTP, SIGTTIN, SIGTTOU};
    default:
      return {};
  }
}
#endif

Status set_signal_handler(SignalType type, void (*func)(int)) {
#ifdef TD_PORT_POSIX
  return set_signal_handler_impl(get_native_signals(type), func == nullptr ? SIG_DFL : func);
#endif
#ifdef TD_PORT_WINDOWS
  return Status::OK();  // nothing to do
#endif
}

using extended_signal_handler = void (*)(int sig, void *addr);
#ifdef TD_PORT_POSIX
static extended_signal_handler extended_signal_handlers[NSIG] = {};

static void siginfo_handler(int signum, siginfo_t *info, void *data) {
  auto handler = extended_signal_handlers[signum];
  handler(signum, info->si_addr);
}
#endif

Status set_extended_signal_handler(SignalType type, extended_signal_handler func) {
#ifdef TD_PORT_POSIX
  CHECK(func != nullptr);
  auto signals = get_native_signals(type);
  for (auto signal : signals) {
    CHECK(0 <= signal && signal < NSIG);
    extended_signal_handlers[signal] = func;
  }
  return set_signal_handler_impl(std::move(signals), siginfo_handler, true);
#endif
#ifdef TD_PORT_WINDOWS
  return Status::OK();  // nothing to do
#endif
}

Status set_runtime_signal_handler(int runtime_signal_number, void (*func)(int)) {
#ifdef SIGRTMIN
  CHECK(SIGRTMIN + runtime_signal_number <= SIGRTMAX);
  return set_signal_handler_impl({SIGRTMIN + runtime_signal_number}, func == nullptr ? SIG_DFL : func);
#else
  return Status::OK();
#endif
}

Status ignore_signal(SignalType type) {
#ifdef TD_PORT_POSIX
  return set_signal_handler_impl(get_native_signals(type), SIG_IGN);
#endif
#ifdef TD_PORT_WINDOWS
  return Status::OK();  // nothing to do
#endif
}

static void signal_safe_append_int(char **s, Slice name, int number) {
  if (number < 0) {
    number = std::numeric_limits<int>::max();
  }

  *--*s = ' ';
  *--*s = ']';

  do {
    *--*s = static_cast<char>(number % 10 + '0');
    number /= 10;
  } while (number > 0);

  *--*s = ' ';

  for (auto pos = static_cast<int>(name.size()) - 1; pos >= 0; pos--) {
    *--*s = name[pos];
  }

  *--*s = '[';
}

static void signal_safe_write_data(Slice data) {
#ifdef TD_PORT_POSIX
  while (!data.empty()) {
    auto res = write(2, data.begin(), data.size());
    if (res < 0 && errno == EINTR) {
      continue;
    }
    if (res <= 0) {
      break;
    }

    if (res > 0) {
      data.remove_prefix(res);
    }
  }
#endif
#ifdef TD_PORT_WINDOWS
// TODO write data
#endif
}

static int get_process_id() {
#ifdef TD_PORT_POSIX
  return getpid();
#endif
#ifdef TD_PORT_WINDOWS
  return GetCurrentProcessId();
#endif
}

void signal_safe_write(Slice data, bool add_header) {
  auto old_errno = errno;

  if (add_header) {
    constexpr size_t HEADER_BUF_SIZE = 100;
    char header[HEADER_BUF_SIZE];
    char *header_end = header + HEADER_BUF_SIZE;
    char *header_begin = header_end;

    signal_safe_append_int(&header_begin, "time", static_cast<int>(std::time(nullptr)));
    signal_safe_append_int(&header_begin, "pid", get_process_id());

    signal_safe_write_data(Slice(header_begin, header_end));
  }

  signal_safe_write_data(data);

  errno = old_errno;
}

void signal_safe_write_signal_number(int sig, bool add_header) {
  char buf[100];
  char *end = buf + sizeof(buf);
  char *ptr = end;
  *--ptr = '\n';
  do {
    *--ptr = static_cast<char>(sig % 10 + '0');
    sig /= 10;
  } while (sig != 0);

  ptr -= 8;
  std::memcpy(ptr, "Signal: ", 8);
  signal_safe_write(Slice(ptr, end), add_header);
}

void signal_safe_write_pointer(void *p, bool add_header) {
  std::uintptr_t addr = reinterpret_cast<std::uintptr_t>(p);
  char buf[100];
  char *end = buf + sizeof(buf);
  char *ptr = end;
  *--ptr = '\n';
  do {
    *--ptr = td::format::hex_digit(addr % 16);
    addr /= 16;
  } while (addr != 0);
  *--ptr = 'x';
  *--ptr = '0';
  ptr -= 9;
  std::memcpy(ptr, "Address: ", 9);
  signal_safe_write(Slice(ptr, end), add_header);
}

}  // namespace td
