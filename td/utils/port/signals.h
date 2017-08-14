#pragma once

#include "td/utils/port/config.h"

#include "td/utils/Status.h"

namespace td {

Status setup_signals_alt_stack();

}  // namespace td
