#pragma once

#include <cassert>
#include <cstdio>
#include <cstring>
#include <cstdint>

#include "td/utils/int_types.h"
#include "td/utils/logging.h"
#include "td/utils/Slice.h"
#include "td/utils/StorerBase.h"

namespace td {
namespace tl {

class tl_storer_unsafe {
  char *buf;

  tl_storer_unsafe(const tl_storer_unsafe &other) = delete;
  tl_storer_unsafe &operator=(const tl_storer_unsafe &other) = delete;

 public:
  explicit tl_storer_unsafe(char *buf) : buf(buf) {
    CHECK((reinterpret_cast<uint64>(buf) & 3) == 0);
  }

  template <class T>
  inline void store_binary(const T &x) {
    *reinterpret_cast<T *>(buf) = x;
    buf += sizeof(x);
  }

  void store_int(int32 x) {
    store_binary<int32>(x);
  }
  void store_long(int64 x) {
    store_binary<int64>(x);
  }

  inline void store_slice(Slice slice) {
    memcpy(buf, slice.begin(), slice.size());
    buf += slice.size();
  }
  inline void store_storer(const Storer &storer) {
    size_t size = storer.store(reinterpret_cast<uint8 *>(buf));
    buf += size;
  }

  template <class T>
  inline void store_string(const T &str) {
    int len = (int)str.size();
    if (len < 254) {
      *buf++ = (char)(len);
      len++;
    } else if (len < (1 << 24)) {
      *buf++ = (char)(254);
      *buf++ = (char)(len & 255);
      *buf++ = (char)((len >> 8) & 255);
      *buf++ = (char)(len >> 16);
    } else {
      assert(0);
    }
    memcpy(buf, str.data(), str.size());
    buf += str.size();

    switch (len & 3) {
      case 1:
        *buf++ = '\0';
      case 2:
        *buf++ = '\0';
      case 3:
        *buf++ = '\0';
    }
  }

  char *get_buf() const {
    return buf;
  }
};

class tl_storer_calc_length {
  size_t length;

  tl_storer_calc_length(const tl_storer_calc_length &other) = delete;
  tl_storer_calc_length &operator=(const tl_storer_calc_length &other) = delete;

 public:
  tl_storer_calc_length() : length(0) {
  }

  template <class T>
  inline void store_binary(const T &x) {
    length += sizeof(x);
  }

  void store_int(int32 x) {
    store_binary<int32>(x);
  }

  void store_long(int64 x) {
    store_binary<int64>(x);
  }

  inline void store_slice(Slice slice) {
    length += slice.size();
  }

  inline void store_storer(const Storer &storer) {
    length += storer.size();
  }

  template <class T>
  inline void store_string(const T &str) {
    size_t add = str.size();
    if (add < 254) {
      add += 1;
    } else {
      add += 4;
    }
    add = (add + 3) & -4;
    length += add;
  }

  int get_length() const {
    // TODO: remove static_cast
    return static_cast<int>(length);
  }
};

class tl_storer_to_string {
  std::string result;
  int shift;

  tl_storer_to_string(const tl_storer_to_string &other) = delete;
  tl_storer_to_string &operator=(const tl_storer_to_string &other) = delete;

  void store_field_begin(const char *name) {
    for (int i = 0; i < shift; i++) {
      result += ' ';
    }
    if (name && name[0]) {
      result += name;
      result += " = ";
    }
  }

  void store_field_end() {
    result += "\n";
  }

  void store_long(int64 value) {
    char buf[64];
    auto len = snprintf(buf, sizeof(buf), "%lld", (long long)value);
    CHECK(static_cast<size_t>(len) < sizeof(buf));
    result += buf;
  }

 public:
  tl_storer_to_string() : result(), shift() {
  }

  void store_field(const char *name, bool value) {
    store_field_begin(name);
    result += (value ? "true" : "false");
    store_field_end();
  }

  void store_field(const char *name, int32_t value) {
    store_field(name, (int64_t)value);
  }

  void store_field(const char *name, int64_t value) {
    store_field_begin(name);
    store_long(value);
    store_field_end();
  }

  void store_field(const char *name, double value) {
    store_field_begin(name);
    char buf[640];
    auto len = snprintf(buf, sizeof(buf), "%lf", value);
    CHECK(static_cast<size_t>(len) < sizeof(buf));
    result += buf;
    store_field_end();
  }

  void store_field(const char *name, const char *value) {
    store_field_begin(name);
    result += value;
    store_field_end();
  }

  void store_field(const char *name, const string &value) {
    store_field_begin(name);
    result += '"';
    result.append(value.data(), value.size());
    result += '"';
    store_field_end();
  }

  template <class T>
  void store_field(const char *name, const T &value) {
    store_field_begin(name);
    result.append(value.data(), value.size());
    store_field_end();
  }

  template <class BytesT>
  void store_bytes_field(const char *name, const BytesT &value) {
    static const char *hex = "0123456789ABCDEF";

    store_field_begin(name);
    result.append("bytes { ");
    for (size_t i = 0; i < value.size(); i++) {
      int b = value[static_cast<int>(i)] & 0xff;
      result += hex[b >> 4];
      result += hex[b & 15];
      result += ' ';
    }
    result.append("}");
    store_field_end();
  }

  void store_field(const char *name, const UInt128 &value) {
    store_field_begin(name);
    store_long(value.low);
    result += " ";
    store_long(value.high);
    store_field_end();
  }

  void store_field(const char *name, const UInt256 &value) {
    store_field_begin(name);
    store_long(value.low.low);
    result += " ";
    store_long(value.low.high);
    result += " ";
    store_long(value.high.low);
    result += " ";
    store_long(value.high.high);
    store_field_end();
  }

  void store_class_begin(const char *field_name, const char *class_name) {
    store_field_begin(field_name);
    result += class_name;
    result += " {\n";
    shift += 2;
  }

  void store_class_end() {
    shift -= 2;
    for (int i = 0; i < shift; i++) {
      result += ' ';
    }
    result += "}\n";
    assert(shift >= 0);
  }

  std::string str() const {
    return result;
  }
};

template <class T>
size_t calc_length(const T &data) {
  tl_storer_calc_length storer_calc_length;
  data.store(storer_calc_length);
  return storer_calc_length.get_length();
}

template <class T, class CharT>
size_t store_unsafe(const T &data, CharT *dst) {
  char *start = reinterpret_cast<char *>(dst);
  tl_storer_unsafe storer_unsafe(start);
  data.store(storer_unsafe);
  return storer_unsafe.get_buf() - start;
}

}  // namespace tl
}  // namespace td
