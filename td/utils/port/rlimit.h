#pragma once

#include "td/utils/common.h"
#include "td/utils/Status.h"

namespace td {

enum class ResourceLimitType { NoFile };

Status set_resource_limit(ResourceLimitType type, uint64 value, uint64 max_value = 0);

Status set_maximize_resource_limit(ResourceLimitType type, uint64 value);

}  // namespace td
