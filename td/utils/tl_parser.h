#pragma once

#include "td/utils/buffer.h"
#include "td/utils/common.h"
#include "td/utils/format.h"
#include "td/utils/logging.h"
#include "td/utils/Slice.h"
#include "td/utils/Status.h"
#include "td/utils/StringBuilder.h"
#include "td/utils/utf8.h"

#include <array>
#include <cstring>
#include <limits>
#include <string>

namespace td {
namespace tl {

class tl_parser {
  const int32 *data = nullptr;
  const int32 *data_begin = nullptr;
  int32 data_len;
  std::string error;
  int32 error_pos;

  unique_ptr<int32[]> data_buf;
  static constexpr size_t SMALL_DATA_ARRAY_SIZE = 6;
  std::array<int32, SMALL_DATA_ARRAY_SIZE> small_data_array;

  static const int32 empty_data[8];

  tl_parser(const tl_parser &other) = delete;
  tl_parser &operator=(const tl_parser &other) = delete;

 public:
  tl_parser(const int32 *data, int32 len) : data(data), data_begin(data), data_len(len), error(), error_pos(-1) {
  }

  tl_parser(const char *data, int32 len) : tl_parser(data, static_cast<size_t>(len)) {
    if (len < 0) {
      set_error("Negative length");
    }
  }

  tl_parser(const char *data_ptr, size_t data_ptr_len) : data(), data_begin(), data_len(), error(), error_pos(-1) {
    if (data_ptr_len % sizeof(int32) != 0) {
      set_error("Wrong length");
      return;
    }
    static_assert(sizeof(size_t) >= sizeof(int32), "");
    if (data_ptr_len / sizeof(int32) > static_cast<size_t>(std::numeric_limits<int32>::max())) {
      set_error("Too big length");
      return;
    }

    data_len = static_cast<int32>(data_ptr_len / sizeof(int32));
    if (is_aligned_pointer<4>(data_ptr)) {
      data = reinterpret_cast<const int32 *>(data_ptr);
    } else {
      int32 *buf;
      if (static_cast<size_t>(data_len) <= small_data_array.size()) {
        buf = &small_data_array[0];
      } else {
        LOG(ERROR) << "Unexpected big unaligned data pointer of length " << data_ptr_len << " at " << data_ptr;
        data_buf = make_unique<int32[]>(data_len);
        buf = data_buf.get();
      }
      std::memcpy(static_cast<void *>(buf), static_cast<const void *>(data_ptr), data_ptr_len);
      data = buf;
    }
    data_begin = data;
    error_pos = -1;
  }

  explicit tl_parser(Slice slice) : tl_parser(slice.begin(), slice.size()) {
  }

  void set_error(const string &error_message);

  const char *get_error() const {
    if (error.empty()) {
      return nullptr;
    }
    return error.c_str();
  }

  Status get_status() const {
    if (error.empty()) {
      return Status::OK();
    }
    return Status::Error(PSLICE() << error << " at: " << error_pos);
  }

  int get_error_pos() const {
    return error_pos;
  }

  void check_len(const int len) {
    if (unlikely(data_len < len)) {
      set_error("Not enough data to read");
    } else {
      data_len -= len;
    }
  }

  int32 fetch_int_unsafe() {
    return *data++;
  }

  int32 fetch_int() {
    check_len(1);
    return fetch_int_unsafe();
  }

  int64 fetch_long_unsafe() {
    int64 result = *reinterpret_cast<const int64 *>(data);
    data += 2;
    return result;
  }

  int64 fetch_long() {
    check_len(2);
    return fetch_long_unsafe();
  }

  double fetch_double_unsafe() {
    double result = *reinterpret_cast<const double *>(data);
    data += 2;
    return result;
  }

  double fetch_double() {
    check_len(2);
    return fetch_double_unsafe();
  }

  template <class T>
  T fetch_binary() {
    static_assert(sizeof(T) <= sizeof(empty_data), "too big fetch_binary");
    check_len(static_cast<int>(sizeof(T) / 4));
    T result = *reinterpret_cast<const T *>(data);
    data += sizeof(T) / 4;
    return result;
  }

  void fetch_skip_unsafe(const int len) {
    data += len;
  }

  void fetch_skip(const int len) {
    check_len(len);
    fetch_skip_unsafe(len);
  }

  template <class T>
  T fetch_string() {
    check_len(1);
    const unsigned char *str = reinterpret_cast<const unsigned char *>(data);
    int result_len = *str;
    if (result_len < 254) {
      check_len(result_len >> 2);
      data += (result_len >> 2) + 1;
      return T(reinterpret_cast<const char *>(str + 1), result_len);
    } else if (result_len == 254) {
      result_len = str[1] + (str[2] << 8) + (str[3] << 16);
      check_len((result_len + 3) >> 2);
      data += ((result_len + 7) >> 2);
      return T(reinterpret_cast<const char *>(str + 4), result_len);
    } else {
      set_error("Can't fetch string, 255 found");
      return T();
    }
  }

  template <class T>
  T fetch_string_raw(const int size) {
    const char *result = reinterpret_cast<const char *>(data);
    CHECK(size % 4 == 0);
    check_len(size >> 2);
    data += size >> 2;
    return T(result, size);
  }

  void fetch_end() {
    if (data_len) {
      set_error("Too much data to fetch");
    }
  }

  int32 get_data_len() const {
    return data_len;
  }

  int32 get_pos() const {
    return static_cast<int32>(data - data_begin);
  }

  bool set_pos(int pos) {
    if (!error.empty() || pos < 0 || data_begin + pos > data) {
      return false;
    }

    data_len += static_cast<int32>(data - data_begin - pos);
    data = data_begin + pos;
    return true;
  }

  const char *get_buf() const {
    return reinterpret_cast<const char *>(data);
  }
};

class tl_buffer_parser : public tl_parser {
 public:
  explicit tl_buffer_parser(const BufferSlice *buffer_slice)
      : tl_parser(buffer_slice->as_slice().begin(), buffer_slice->as_slice().size()), parent_(buffer_slice) {
  }
  template <class T>
  T fetch_string() {
    auto result = tl_parser::fetch_string<T>();
    for (auto &c : result) {
      if (c == '\0') {
        c = ' ';
      }
    }
    if (check_utf8(result)) {
      return result;
    }
    CHECK(!result.empty());
    LOG(WARNING) << "Wrong UTF-8 string [[" << result << "]] in " << format::as_hex_dump<4>(parent_->as_slice());

    // try to remove last character
    size_t new_size = result.size() - 1;
    while (new_size != 0 && !is_utf8_character_first_code_unit(static_cast<unsigned char>(result[new_size]))) {
      new_size--;
    }
    result.resize(new_size);
    if (check_utf8(result)) {
      return result;
    }

    return T();
  }
  template <class T>
  T fetch_string_raw(const int size) {
    return tl_parser::fetch_string_raw<T>(size);
  }

 private:
  const BufferSlice *parent_;

  BufferSlice as_buffer_slice(Slice slice) {
    if (is_aligned_pointer<4>(slice.data())) {
      return parent_->from_slice(slice);
    }
    return BufferSlice(slice);
  }
};

template <>
inline BufferSlice tl_buffer_parser::fetch_string<BufferSlice>() {
  return as_buffer_slice(tl_parser::fetch_string<Slice>());
}

template <>
inline BufferSlice tl_buffer_parser::fetch_string_raw<BufferSlice>(const int size) {
  return as_buffer_slice(tl_parser::fetch_string_raw<Slice>(size));
}

}  // namespace tl
}  // namespace td
