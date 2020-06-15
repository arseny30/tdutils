#pragma once

#include "td/utils/Slice.h"
#include "td/utils/Status.h"

namespace td {

Status change_user(CSlice username, CSlice groupname = CSlice());

}
