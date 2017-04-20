#pragma once
#include "td/utils/common.h"

#include <cstring>
#include <type_traits>

namespace td {
class Slice;

class MutableSlice {
 private:
  char *s_;
  size_t len_;

  struct private_tag {};

 public:
  MutableSlice();
  MutableSlice(void *s, size_t len);
  MutableSlice(string &s);
  template <class T>
  explicit MutableSlice(T s, std::enable_if_t<std::is_same<char *, T>::value, private_tag> = {})
      : MutableSlice(s, strlen(s)) {
  }
  explicit MutableSlice(const Slice &from);
  MutableSlice(void *s, void *t);
  template <size_t N>
  constexpr MutableSlice(char (&a)[N]) : s_(a), len_(N - 1) {
  }

  void clear();

  bool empty() const;
  size_t size() const;

  MutableSlice &remove_prefix(size_t prefix_len);
  MutableSlice &remove_suffix(size_t suffix_len);
  MutableSlice &truncate(size_t size);
  MutableSlice &rtruncate(size_t size);

  MutableSlice copy();

  char *data() const;
  char *begin() const;
  unsigned char *ubegin() const;
  char *end() const;
  unsigned char *uend() const;

  string str() const;
  MutableSlice substr(size_t from) const;
  MutableSlice substr(size_t from, size_t size) const;
  size_t find(char c) const;
  size_t rfind(char c) const;

  void copy_from(Slice from);

  char &back();
  char &operator[](size_t i);
};

class Slice {
 private:
  const char *s_;
  size_t len_;

  struct private_tag {};

 public:
  Slice();
  Slice(const MutableSlice &other);
  Slice(const void *s, size_t len);
  Slice(const string &s);
  Slice(const std::vector<unsigned char> &v);
  Slice(const std::vector<char> &v);
  template <class T>
  explicit Slice(T s, std::enable_if_t<std::is_same<char *, std::remove_const_t<T>>::value, private_tag> = {})
      : Slice(s, strlen(s)) {
  }
  template <class T>
  explicit Slice(T s, std::enable_if_t<std::is_same<const char *, std::remove_const_t<T>>::value, private_tag> = {})
      : Slice(s, strlen(s)) {
  }
  Slice(const void *s, const void *t);

  template <size_t N>
  constexpr Slice(char (&a)[N]) = delete;

  template <size_t N>
  constexpr Slice(const char (&a)[N]) : s_(a), len_(N - 1) {
  }

  void clear();

  bool empty() const;
  size_t size() const;

  Slice &remove_prefix(size_t prefix_len);
  Slice &remove_suffix(size_t suffix_len);
  Slice &truncate(size_t size);
  Slice &rtruncate(size_t size);

  Slice copy();

  const char *data() const;
  const char *begin() const;
  const unsigned char *ubegin() const;
  const char *end() const;
  const unsigned char *uend() const;

  string str() const;
  Slice substr(size_t from) const;
  Slice substr(size_t from, size_t size) const;
  size_t find(char c) const;
  size_t rfind(char c) const;

  char back() const;
  char operator[](size_t i) const;
};

bool operator==(const Slice &a, const Slice &b);
bool operator!=(const Slice &a, const Slice &b);

template <class SliceT>
SliceT trim(SliceT slice);

class MutableCSlice : public MutableSlice {
  struct private_tag {};

 public:
  MutableCSlice() = delete;
  MutableCSlice(const MutableCSlice &other) : MutableSlice(other) {
  }
  MutableCSlice(string &s) : MutableSlice(s) {
  }
  template <class T>
  explicit MutableCSlice(T s, std::enable_if_t<std::is_same<char *, T>::value, private_tag> = {}) : MutableSlice(s) {
  }
  MutableCSlice(void *s, void *t);

  template <size_t N>
  constexpr MutableCSlice(char (&a)[N]) = delete;

  const char *c_str() const {
    return begin();
  }
};

class CSlice : public Slice {
  struct private_tag {};

 public:
  explicit CSlice(const MutableSlice &other) : Slice(other) {
  }
  CSlice(const MutableCSlice &other) : Slice(other.begin(), other.size()) {
  }
  CSlice(const string &s) : Slice(s) {
  }
  template <class T>
  explicit CSlice(T s, std::enable_if_t<std::is_same<char *, std::remove_const_t<T>>::value, private_tag> = {})
      : Slice(s) {
  }
  template <class T>
  explicit CSlice(T s, std::enable_if_t<std::is_same<const char *, std::remove_const_t<T>>::value, private_tag> = {})
      : Slice(s) {
  }
  CSlice(const char *s, const char *t);

  template <size_t N>
  constexpr CSlice(char (&a)[N]) = delete;

  template <size_t N>
  constexpr CSlice(const char (&a)[N]) : Slice(a) {
  }

  CSlice() : CSlice("") {
  }

  const char *c_str() const {
    return begin();
  }
};

// class String {
// public:
// size_t size() {
// return size_ & ~MASK;
//}
// bool is_owned() {
// return size_ & MASK;
//}
//~String() {
// if (is_owned()) {
//}
//}

// private:
// size_t size_;
// const char *ptr_;
// static constexpr size_t MASK = static_cast<size_t>(1) << (sizeof(size_t) * 8 - 1);
//};

}  // namespace td
