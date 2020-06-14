#pragma once

#include "td/utils/common.h"
#include "td/utils/Status.h"

namespace td {

enum class ResourceLimitType { NoFile };

Status set_resource_limit(ResourceLimitType type, uint64 value);

}  // namespace td
