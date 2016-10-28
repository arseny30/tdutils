#pragma once

#include "td/utils/buffer.h"
#include "td/utils/logging.h"
#include "td/utils/Slice.h"
#include "td/utils/Status.h"
#include "td/utils/StringBuilder.h"
#include "td/utils/utf8.h"

#include <cassert>
#include <cstddef>
#include <cstdint>

#include <limits>
#include <string>

namespace td {
namespace tl {

class tl_parser {
  const int32_t *data;
  const int32_t *data_begin;
  int data_len;
  std::string error;
  int error_pos;

  static const int32_t empty_data[8];

  tl_parser(const tl_parser &other) = delete;
  tl_parser &operator=(const tl_parser &other) = delete;

 public:
  tl_parser(const int32_t *data, int len) : data(data), data_begin(data), data_len(len), error(), error_pos(-1) {
    CHECK((reinterpret_cast<uint64>(data) & 3) == 0);
  }

  tl_parser(const char *data, int len) : tl_parser(reinterpret_cast<const int32_t *>(data), len / 4) {
    if (len % 4 != 0) {
      set_error("Wrong length");
    }
  }

  tl_parser(const char *data, size_t len)
      : tl_parser(reinterpret_cast<const int32_t *>(data), static_cast<int>(len / 4)) {
    if (len % 4 != 0) {
      set_error("Wrong length");
    }
    assert(sizeof(size_t) >= sizeof(int));
    if (len / 4 > static_cast<size_t>(std::numeric_limits<int>::max())) {
      set_error("Too big length");
    }
  }

  explicit tl_parser(Slice slice) : tl_parser(slice.begin(), slice.size()) {
  }

  void set_error(const string &error_message);

  inline const char *get_error(void) const {
    if (error.empty()) {
      return nullptr;
    }
    return error.c_str();
  }

  inline Status get_status() const {
    if (error.empty()) {
      return Status::OK();
    }
    return Status::Error(PSTR() << error << " at: " << error_pos);
  }

  inline int get_error_pos(void) const {
    return error_pos;
  }

  inline void check_len(const int len) {
    if (unlikely(data_len < len)) {
      set_error("Not enough data to read");
    } else {
      data_len -= len;
    }
  }

  inline int32_t fetch_int_unsafe(void) {
    return *data++;
  }

  inline int32_t fetch_int(void) {
    check_len(1);
    return fetch_int_unsafe();
  }

  inline int64_t fetch_long_unsafe(void) {
    int64_t result = *reinterpret_cast<const int64_t *>(data);
    data += 2;
    return result;
  }

  inline int64_t fetch_long(void) {
    check_len(2);
    return fetch_long_unsafe();
  }

  inline double fetch_double_unsafe(void) {
    double result = *reinterpret_cast<const double *>(data);
    data += 2;
    return result;
  }

  inline double fetch_double(void) {
    check_len(2);
    return fetch_double_unsafe();
  }

  template <class T>
  inline T fetch_binary() {
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
  inline T fetch_string(void) {
    check_len(1);
    const uint8_t *str = reinterpret_cast<const uint8_t *>(data);
    int result_len = (uint8_t)*str;
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
  inline T fetch_string_raw(const int size) {
    const uint8_t *str = reinterpret_cast<const uint8_t *>(data);
    CHECK(size % 4 == 0);
    check_len(size >> 2);
    data += size >> 2;
    return T(reinterpret_cast<const char *>(str), size);
  }

  inline void fetch_end(void) {
    if (data_len) {
      set_error("Too much data to fetch");
    }
  }

  inline int get_pos(void) const {
    return (int)(data - data_begin);
  }

  inline bool set_pos(int pos) {
    if (!error.empty() || pos < 0 || data_begin + pos > data) {
      return false;
    }

    data_len += (int)(data - data_begin - pos);
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
      : tl_parser(buffer_slice->as_slice().begin(), static_cast<int32>(buffer_slice->as_slice().size()))
      , parent_(buffer_slice) {
  }
  template <class T>
  inline T fetch_string(void) {
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
    LOG(WARNING) << "Wrong UTF-8 string [[" << result << "]]";

    // try to remove last character
    size_t new_size = result.size() - 1;
    while (new_size != 0 && (((unsigned char)result[new_size]) & 0xc0) == 0x80) {
      new_size--;
    }
    result.resize(new_size);
    if (check_utf8(result)) {
      return result;
    }

    return T();
  }
  template <class T>
  inline T fetch_string_raw(const int size) {
    return tl_parser::fetch_string_raw<T>(size);
  }

 private:
  const BufferSlice *parent_;
};

template <>
inline BufferSlice tl_buffer_parser::fetch_string<BufferSlice>() {
  return parent_->from_slice(tl_parser::fetch_string<Slice>());
}

template <>
inline BufferSlice tl_buffer_parser::fetch_string_raw<BufferSlice>(const int size) {
  return parent_->from_slice(tl_parser::fetch_string_raw<Slice>(size));
}

}  // namespace tl
}  // namespace td
