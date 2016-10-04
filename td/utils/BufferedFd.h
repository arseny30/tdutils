#pragma once

#include "td/utils/port/SocketFd.h"
#include "td/utils/port/ServerSocketFd.h"
#include "td/utils/port/EventFd.h"

#include "td/utils/buffer.h"
#include "td/utils/logging.h"
#include "td/utils/format.h"

namespace td {

template <class FdT>
class BufferedFd : public FdT {
 private:
  ChainBufferWriter input_writer_;
  ChainBufferReader input_reader_;
  ChainBufferWriter output_writer_;
  ChainBufferReader output_reader_;
  void init();

 public:
  BufferedFd();
  BufferedFd(FdT &&fd_);
  BufferedFd(BufferedFd &&) = default;
  BufferedFd &operator=(BufferedFd &&) = default;

  void close();

  Result<size_t> flush_read(size_t max_size = static_cast<size_t>(-1)) WARN_UNUSED_RESULT;
  Result<size_t> flush_write() WARN_UNUSED_RESULT;

  bool need_flush_write(size_t at_least = 0) {
    output_reader_.sync_with_writer();
    return output_reader_.size() > at_least;
  }
  size_t ready_for_flush_write() {
    output_reader_.sync_with_writer();
    return output_reader_.size();
  }

  // Yep, direct access to buffers. It is IO interface too.
  ChainBufferReader &input_buffer();
  ChainBufferWriter &output_buffer();
};

// IMPLEMENTATION

/*** BufferedFd ***/
template <class FdT>
void BufferedFd<FdT>::init() {
  input_reader_ = input_writer_.extract_reader();
  output_reader_ = output_writer_.extract_reader();
}

template <class FdT>
BufferedFd<FdT>::BufferedFd() {
  init();
}

template <class FdT>
BufferedFd<FdT>::BufferedFd(FdT &&fd_)
    : FdT(std::move(fd_)) {
  init();
}

template <class FdT>
void BufferedFd<FdT>::close() {
  FdT::close();
  // TODO: clear buffers
}

template <class FdT>
Result<size_t> BufferedFd<FdT>::flush_read(size_t max_read) {
  size_t result = 0;
  while (::td::can_read(*this) && max_read) {
    MutableSlice slice = input_writer_.prepare_append().truncate(max_read);
    TRY_RESULT(x, FdT::read(slice));
    slice.truncate(x);
    input_writer_.confirm_append(x);
    result += x;
    max_read -= x;
  }
  if (result) {
    // TODO: faster sync is possible if you owns writer.
    input_reader_.sync_with_writer();
    LOG(DEBUG) << "flush_read: +" << format::as_size(result) << tag("total", format::as_size(input_reader_.size()));
  }
  return result;
}

template <class FdT>
Result<size_t> BufferedFd<FdT>::flush_write() {
  size_t result = 0;
  // TODO: sync on demand
  output_reader_.sync_with_writer();
  while (!output_reader_.empty() && ::td::can_write(*this)) {
    Slice slice = output_reader_.prepare_read();
    TRY_RESULT(x, FdT::write(slice));
    output_reader_.confirm_read(x);
    result += x;
  }
  if (result) {
    LOG(DEBUG) << "flush_write: +" << format::as_size(result) << tag("left", format::as_size(output_reader_.size()));
  }
  return result;
}

// Yep, direct access to buffers. It is IO interface too.
template <class FdT>
ChainBufferReader &BufferedFd<FdT>::input_buffer() {
  return input_reader_;
}

template <class FdT>
ChainBufferWriter &BufferedFd<FdT>::output_buffer() {
  return output_writer_;
}

}  // end of namespace td
