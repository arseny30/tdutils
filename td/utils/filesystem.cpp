#include "td/utils/port/FileFd.h"
#include "td/utils/buffer.h"
#include "td/utils/Status.h"
#include "td/utils/Slice.h"

namespace td {
Result<BufferSlice> read_file(CSlice path, off_t size) {
  TRY_RESULT(from_file, FileFd::open(path, FileFd::Read));
  if (size == -1) {
    size = from_file.stat().size_;
  }
  BufferWriter content{static_cast<size_t>(size), 0, 0};
  TRY_RESULT(got_size, from_file.read(content.as_slice()));
  if (got_size != static_cast<size_t>(size)) {
    return Status::Error("Failed to read file");
  }
  from_file.close();
  return content.as_buffer_slice();
}

// Very straightforward function. Don't expect much of it.
Status copy_file(CSlice from, CSlice to, off_t size) {
  TRY_RESULT(content, read_file(from, size));
  TRY_RESULT(to_file, FileFd::open(to, FileFd::Truncate | FileFd::Create | FileFd::Write));
  TRY_RESULT(written, to_file.write(content.as_slice()));
  if (written != static_cast<size_t>(size)) {
    return Status::Error("Failed to write file");
  }
  to_file.close();
  return Status::OK();
}
}
