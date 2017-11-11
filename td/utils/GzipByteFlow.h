#pragma once

#include "td/utils/ByteFlow.h"
#include "td/utils/Gzip.h"

namespace td {

#if TD_HAS_ZLIB
class GzipByteFlow final : public ByteFlowBase {
 public:
  GzipByteFlow() = default;

  explicit GzipByteFlow(Gzip::Mode mode) {
    gzip_.init(mode).ensure();
  }

  void init_decode() {
    gzip_.init_decode().ensure();
  }

  void init_encode() {
    gzip_.init_encode().ensure();
  }

  void loop() override;

 private:
  Gzip gzip_;
  size_t uncommited_size_ = 0;
  static constexpr size_t MIN_UPDATE_SIZE = 1 << 14;
};
#endif

}  // namespace td
