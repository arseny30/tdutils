#pragma once

#include "td/utils/common.h"
#include "td/utils/logging.h"
#include "td/utils/Slice.h"

#include <atomic>
#include <cstring>

namespace td {
template <int buffer_size = 32 * (1 << 10)>
class MemoryLog : public LogInterface {
  static constexpr size_t MAX_OUTPUT_SIZE = buffer_size / 16 < (8 << 10) ? buffer_size / 16 : (8 << 10);

 public:
  enum { BufferSize = buffer_size };

  MemoryLog() {
    memset(buffer_, ' ', sizeof(buffer_));
  }

  void append(CSlice new_slice, int log_level) override {
    auto slice = new_slice;
    if (!slice.empty() && slice.back() == '\n') {
      slice.remove_suffix(1);
    }
    slice.truncate(MAX_OUTPUT_SIZE);
    size_t slice_size = slice.size();
    CHECK(slice_size * 3 < buffer_size);
    size_t pad_size = ((slice_size + 15) & ~15) - slice_size;
    size_t magic_size = 16;
    uint32 total_size = static_cast<uint32>(slice_size + pad_size + magic_size);
    uint32 real_pos = pos_.fetch_add(total_size, std::memory_order_relaxed);

    uint32 start_pos = real_pos & (buffer_size - 1);
    uint32 end_pos = start_pos + total_size;
    if (likely(end_pos <= buffer_size)) {
      memcpy(&buffer_[start_pos + magic_size], slice.data(), slice_size);
      memcpy(&buffer_[start_pos + magic_size + slice_size], "                     ", pad_size);
    } else {
      size_t first = buffer_size - start_pos - magic_size;
      size_t second = slice_size - first;
      memcpy(&buffer_[start_pos + magic_size], slice.data(), first);
      memcpy(&buffer_[0], slice.data() + first, second);
      memcpy(&buffer_[second], "                            ", pad_size);
    }

    snprintf(&buffer_[start_pos], 16, "\nLOG:%08x:  ", real_pos);
  }
  Slice get_buffer() {
    return Slice(buffer_, sizeof(buffer_));
  }
  size_t get_pos() {
    return pos_ & (buffer_size - 1);
  }
  void rotate() override {
  }

 private:
  char buffer_[buffer_size];
  std::atomic<uint32> pos_;
};
}  // namespace td
