#pragma once
#include "td/utils/Status.h"
#include "td/utils/Slice.h"
#include "td/utils/buffer.h"

#include <zlib.h>

namespace td {
class Gzip {
 public:
  Gzip(Gzip &&other)
      : stream_(other.stream_)
      , input_size_(other.input_size_)
      , output_size_(other.output_size_)
      , close_input_flag_(other.close_input_flag_)
      , mode_(other.mode_) {
    other.mode_ = Empty;
  }
  Gzip() = default;

  Gzip &operator=(Gzip &&other) {
    clear();
    stream_ = other.stream_;
    input_size_ = other.input_size_;
    output_size_ = other.output_size_;
    close_input_flag_ = other.close_input_flag_;
    mode_ = other.mode_;
    other.mode_ = Empty;
    return *this;
  }

  Gzip(const Gzip &) = delete;
  Gzip &operator=(const Gzip &) = delete;

  enum Mode { Empty, Encode, Decode };
  Status init(Mode mode) WARN_UNUSED_RESULT {
    if (mode == Encode) {
      return init_encode();
    } else if (mode == Decode) {
      return init_decode();
    }
    return Status::OK();
  }
  Status init_encode() WARN_UNUSED_RESULT;
  Status init_decode() WARN_UNUSED_RESULT;
  void set_input(Slice input);
  void set_output(MutableSlice output);
  void close_input() {
    close_input_flag_ = true;
  }
  bool need_input() {
    return stream_.avail_in == 0;
  }
  bool need_output() {
    return stream_.avail_out == 0;
  }
  size_t left_input() {
    return stream_.avail_in;
  }
  size_t left_output() {
    return stream_.avail_out;
  }
  size_t used_input() {
    return input_size_ - left_input();
  }
  size_t used_output() {
    return output_size_ - left_output();
  }
  size_t flush_input() {
    auto res = used_input();
    input_size_ = left_input();
    return res;
  }
  size_t flush_output() {
    auto res = used_output();
    output_size_ = left_output();
    return res;
  }

  enum State { Running, Done };
  Result<State> run() WARN_UNUSED_RESULT;

  ~Gzip() {
    clear();
  }

 private:
  z_stream stream_;
  size_t input_size_;
  size_t output_size_;
  bool close_input_flag_;
  Mode mode_ = Empty;

  void init_common();
  void clear();
};

BufferSlice gzdecode(Slice s);

BufferSlice gzencode(Slice s, double k = 0.9);
}  // namespace td
