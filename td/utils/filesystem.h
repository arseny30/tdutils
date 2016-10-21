#pragma once
#include "td/utils/buffer.h"
#include "td/utils/Status.h"
#include "td/utils/Slice.h"

namespace td {
Result<BufferSlice> read_file(CSlice path, off_t size = -1);
Status copy_file(CSlice from, CSlice to, off_t size = -1);
}
