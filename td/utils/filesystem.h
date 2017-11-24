#pragma once

#include "td/utils/buffer.h"
#include "td/utils/Slice.h"
#include "td/utils/Status.h"

namespace td {

Result<BufferSlice> read_file(CSlice path, int64 size = -1);

Status copy_file(CSlice from, CSlice to, int64 size = -1);

Status write_file(CSlice to, Slice data);

}  // namespace td
