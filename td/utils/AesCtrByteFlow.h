#pragma once

#include "td/utils/ByteFlow.h"
#include "td/utils/common.h"
#include "td/utils/crypto.h"
#include "td/utils/Slice.h"
#include "td/utils/Status.h"
#include "td/utils/UInt.h"

namespace td {

#if TD_HAVE_OPENSSL
class AesCtrByteFlow : public ByteFlowInplaceBase {
 public:
  void init(const UInt256 &key, const UInt128 &iv) {
    state_.init(as_slice(key), as_slice(iv));
  }
  void init(AesCtrState &&state) {
    state_ = std::move(state);
  }
  AesCtrState move_aes_ctr_state() {
    return std::move(state_);
  }
  bool loop() override {
    bool result = false;
    auto ready = input_->prepare_read();
    if (!ready.empty()) {
      state_.encrypt(ready, MutableSlice(const_cast<char *>(ready.data()), ready.size()));
      input_->confirm_read(ready.size());
      output_.advance_end(ready.size());
      result = true;
    }

    if (!is_input_active_) {
      finish(Status::OK());  // End of input stream.
    }
    return result;
  }

 private:
  AesCtrState state_;
};
#endif

}  // namespace td
