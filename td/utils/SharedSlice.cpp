#include "td/utils/SharedSlice.h"

#include "td/utils/buffer.h"

namespace td {

BufferSlice SharedSlice::clone_as_buffer_slice() const {
  return BufferSlice{as_slice()};
}

}  // namespace td
