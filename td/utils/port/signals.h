#pragma once

#include "td/utils/port/config.h"

#include "td/utils/common.h"
#include "td/utils/Slice.h"
#include "td/utils/Status.h"

namespace td {

Status setup_signals_alt_stack() WARN_UNUSED_RESULT;

enum class SignalType { Abort, Error, Quit, Pipe, HangUp, User, Other };

Status set_signal_handler(SignalType type, void (*func)(int sig)) WARN_UNUSED_RESULT;

Status set_extended_signal_handler(SignalType type, void (*func)(int sig, void *addr)) WARN_UNUSED_RESULT;

Status set_runtime_signal_handler(int runtime_signal_number, void (*func)(int sig)) WARN_UNUSED_RESULT;

Status ignore_signal(SignalType type) WARN_UNUSED_RESULT;

// writes data to the standard error stream in a signal-safe way
void signal_safe_write(Slice data, bool add_header = true);

void signal_safe_write_signal_number(int sig, bool add_header = true);

void signal_safe_write_pointer(void *p, bool add_header = true);

}  // namespace td