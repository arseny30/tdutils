#pragma once

#include "td/utils/common.h"
#include "td/utils/port/FileFd.h"
#include "td/utils/Slice.h"
#include "td/utils/Status.h"

#include <cstring>

namespace td {
class BufferedReader {
 public:
  explciit BufferedReader(FileFd &file, size_t buff_size = 8152)
      : file_(file), buff_(buff_size), begin_pos_(0), end_pos_(0) {
  }

  Result<size_t> read(MutableSlice slice) TD_WARN_UNUSED_RESULT;

 private:
  FileFd &file_;
  vector<char> buff_;
  size_t begin_pos_;
  size_t end_pos_;
};

inline Result<size_t> BufferedReader::read(MutableSlice slice) {
  size_t available = end_pos_ - begin_pos_;
  if (available >= slice.size()) {
    // have enough data in buffer
    std::memcpy(slice.begin(), &buff_[begin_pos_], slice.size());
    begin_pos_ += slice.size();
    return slice.size();
  }

  if (available) {
    std::memcpy(slice.begin(), &buff_[begin_pos_], available);
    begin_pos_ += available;
    slice.remove_prefix(available);
  }

  if (slice.size() > buff_.size() / 2) {
    TRY_RESULT(result, file_.read(slice));
    return result + available;
  }

  TRY_RESULT(result, file_.read({&buff_[0], buff_.size()}));
  begin_pos_ = 0;
  end_pos_ = result;

  size_t left = min(end_pos_, slice.size());
  std::memcpy(slice.begin(), &buff_[begin_pos_], left);
  begin_pos_ += left;
  return left + available;
}
}  // namespace td
