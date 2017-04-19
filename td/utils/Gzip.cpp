#include "td/utils/Gzip.h"

#include <cstring>
#include <limits>

namespace td {
Status Gzip::init_encode() {
  CHECK(mode_ == Empty);
  init_common();
  mode_ = Encode;
  int ret = deflateInit2(&stream_, 6, Z_DEFLATED, 15, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY);
  if (ret != Z_OK) {
    return Status::Error("zlib deflate init failed");
  }
  return Status::OK();
}
Status Gzip::init_decode() {
  CHECK(mode_ == Empty);
  init_common();
  mode_ = Decode;
  int ret = inflateInit2(&stream_, MAX_WBITS + 32);
  if (ret != Z_OK) {
    return Status::Error("zlib inflate init failed");
  }
  return Status::OK();
}
void Gzip::set_input(Slice input) {
  CHECK(input_size_ == 0);
  CHECK(!close_input_flag_);
  CHECK(input.size() <= std::numeric_limits<uInt>::max());
  CHECK(stream_.avail_in == 0);
  input_size_ = input.size();
  stream_.avail_in = static_cast<uInt>(input.size());
  stream_.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(input.data()));
}
void Gzip::set_output(MutableSlice output) {
  CHECK(output_size_ == 0);
  CHECK(output.size() <= std::numeric_limits<uInt>::max());
  CHECK(stream_.avail_out == 0);
  output_size_ = output.size();
  stream_.avail_out = static_cast<uInt>(output.size());
  stream_.next_out = reinterpret_cast<Bytef *>(output.data());
}
Result<Gzip::State> Gzip::run() {
  while (true) {
    int ret;
    if (mode_ == Decode) {
      ret = inflate(&stream_, Z_NO_FLUSH);
    } else {
      ret = deflate(&stream_, close_input_flag_ ? Z_FINISH : Z_NO_FLUSH);
    }

    if (ret == Z_OK) {
      return Running;
    }
    if (ret == Z_STREAM_END) {
      // TODO(now): fail if input is not empty;
      clear();
      return Done;
    }
    clear();
    return Status::Error(PSLICE() << "zlib error " << ret);
  }
}
void Gzip::init_common() {
  std::memset(&stream_, 0, sizeof(stream_));
  stream_.zalloc = Z_NULL;
  stream_.zfree = Z_NULL;
  stream_.opaque = Z_NULL;
  stream_.avail_in = 0;
  stream_.next_in = nullptr;
  stream_.avail_out = 0;
  stream_.next_out = nullptr;

  input_size_ = 0;
  output_size_ = 0;

  close_input_flag_ = false;
}
void Gzip::clear() {
  if (mode_ == Decode) {
    inflateEnd(&stream_);
  } else if (mode_ == Encode) {
    deflateEnd(&stream_);
  }
  mode_ = Empty;
}

BufferSlice gzdecode(Slice s) {
  Gzip gzip;
  gzip.init_decode().ensure();
  auto message = ChainBufferWriter::create_empty();
  gzip.set_input(s);
  gzip.close_input();
  double k = 2;
  gzip.set_output(message.prepare_append(static_cast<size_t>(static_cast<double>(s.size()) * k)));
  while (true) {
    auto r_state = gzip.run();
    if (r_state.is_error()) {
      return BufferSlice();
    }
    auto state = r_state.ok();
    if (state == Gzip::Done) {
      message.confirm_append(gzip.flush_output());
      break;
    }
    if (gzip.need_input()) {
      return BufferSlice();
    }
    if (gzip.need_output()) {
      message.confirm_append(gzip.flush_output());
      k *= 1.5;
      gzip.set_output(message.prepare_append(static_cast<size_t>(static_cast<double>(gzip.left_input()) * k)));
    }
  }
  return message.extract_reader().move_as_buffer_slice();
}

BufferSlice gzencode(Slice s, double k) {
  Gzip gzip;
  gzip.init_encode().ensure();
  gzip.set_input(s);
  gzip.close_input();
  size_t max_size = static_cast<size_t>(static_cast<double>(s.size()) * k);
  BufferWriter message{max_size};
  gzip.set_output(message.prepare_append());
  auto r_state = gzip.run();
  if (r_state.is_error()) {
    return BufferSlice();
  }
  auto state = r_state.ok();
  if (state != Gzip::Done) {
    return BufferSlice();
  }
  message.confirm_append(gzip.flush_output());
  return message.as_buffer_slice();
}
}  // namespace td
