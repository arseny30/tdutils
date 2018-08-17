#pragma once
#include "td/utils/port/FileFd.h"

namespace td {

FileFd &Stdin();
FileFd &Stdout();
FileFd &Stderr();

}  // namespace td
