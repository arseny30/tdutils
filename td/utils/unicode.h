#pragma once

#include "td/utils/common.h"

namespace td {

enum class UnicodeSimpleCategory { Unknown, Letter, DecimalNumber, Number, Separator };

UnicodeSimpleCategory get_unicode_simple_category(uint32 code);

}  // end of namespace td
