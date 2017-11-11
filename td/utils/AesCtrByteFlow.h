#pragma once

#include "td/utils/ByteFlow.h"
#include "td/utils/crypto.h"

namespace td {

#if TD_HAS_OPENSSL
class AesCtrByteFlow : public ByteFlowInplaceBase {
 public:
  void init(const UInt256 &key, const UInt128 &iv) {
    key_ = key;
    init_aes_ctr_state(key, iv, &state_);
  }
  void init(const UInt256 &key, AesCtrState &&state) {
    key_ = key;
    state_ = std::move(state);
  }
  AesCtrState move_aes_ctr_state() {
    return std::move(state_);
  }
  void loop() override {
    bool was_updated = false;
    while (true) {
      auto ready = input_->prepare_read();
      if (ready.empty()) {
        break;
      }
      aes_ctr_encrypt(&state_, ready, MutableSlice(const_cast<char *>(ready.data()), ready.size()));
      input_->confirm_read(ready.size());
      output_.advance_end(ready.size());
      was_updated = true;
    }
    if (was_updated) {
      on_output_updated();
    }
    if (!is_input_active_) {
      finish(Status::OK());  // End of input stream.
    }
    set_need_size(1);
  }

 private:
  UInt256 key_;
  AesCtrState state_;
};
#endif

}  // namespace td
