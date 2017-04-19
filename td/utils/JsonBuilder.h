#pragma once

#include "td/utils/common.h"
#include "td/utils/format.h"
#include "td/utils/logging.h"
#include "td/utils/misc.h"
#include "td/utils/Parser.h"
#include "td/utils/ScopeGuard.h"
#include "td/utils/Slice.h"
#include "td/utils/Status.h"
#include "td/utils/StringBuilder.h"

#include <tuple>
#include <type_traits>
#include <utility>

namespace td {

class JsonTrue {
 public:
  friend StringBuilder &operator<<(StringBuilder &sb, const JsonTrue &val) {
    return sb << "true";
  }
};

class JsonFalse {
 public:
  friend StringBuilder &operator<<(StringBuilder &sb, const JsonFalse &val) {
    return sb << "false";
  }
};

class JsonBool {
 public:
  explicit JsonBool(bool value) : value_(value) {
  }
  friend StringBuilder &operator<<(StringBuilder &sb, const JsonBool &val) {
    if (val.value_) {
      return sb << JsonTrue();
    } else {
      return sb << JsonFalse();
    }
  }

 private:
  bool value_;
};

class JsonInt {
 public:
  explicit JsonInt(int32 value) : value_(value) {
  }
  friend StringBuilder &operator<<(StringBuilder &sb, const JsonInt &val) {
    return sb << val.value_;
  }

 private:
  int32 value_;
};

class JsonLong {
 public:
  explicit JsonLong(int64 value) : value_(value) {
  }
  friend StringBuilder &operator<<(StringBuilder &sb, const JsonLong &val) {
    return sb << val.value_;
  }

 private:
  int64 value_;
};

class JsonFloat {
 public:
  explicit JsonFloat(double value) : value_(value) {
  }
  friend StringBuilder &operator<<(StringBuilder &sb, const JsonFloat &val) {
    return sb << val.value_;
  }

 private:
  double value_;
};

class JsonOneChar {
 public:
  explicit JsonOneChar(unsigned int c) : c_(c) {
  }

  friend StringBuilder &operator<<(StringBuilder &sb, const JsonOneChar &val) {
    auto c = val.c_;
    return sb << '\\' << 'u' << "0123456789abcdef"[c >> 12] << "0123456789abcdef"[(c >> 8) & 15]
              << "0123456789abcdef"[(c >> 4) & 15] << "0123456789abcdef"[c & 15];
  }

 private:
  unsigned int c_;
};

class JsonChar {
 public:
  explicit JsonChar(unsigned int c) : c_(c) {
  }
  friend StringBuilder &operator<<(StringBuilder &sb, const JsonChar &val) {
    auto c = val.c_;
    if (c < 0x10000) {
      if (0xD7FF < c && c < 0xE000) {
        // UTF-8 correctness already checked
        UNREACHABLE();
      }
      return sb << JsonOneChar(c);
    } else if (c <= 0x10ffff) {
      return sb << JsonOneChar(0xD7C0 + (c >> 10)) << JsonOneChar(0xDC00 + (c & 0x3FF));
    } else {
      // UTF-8 correctness already checked
      UNREACHABLE();
    }
  }

 private:
  unsigned int c_;
};

class JsonRaw {
 public:
  explicit JsonRaw(Slice value) : value_(value) {
  }
  friend StringBuilder &operator<<(StringBuilder &sb, const JsonRaw &val) {
    return sb << val.value_;
  }

 private:
  Slice value_;
};

class JsonRawString {
 public:
  explicit JsonRawString(Slice value) : value_(value) {
  }
  friend StringBuilder &operator<<(StringBuilder &sb, const JsonRawString &val);

 private:
  Slice value_;
};

class JsonString {
 public:
  explicit JsonString(Slice str) : str_(str) {
  }

  friend StringBuilder &operator<<(StringBuilder &sb, const JsonString &val);

 private:
  Slice str_;
};

class JsonScope;
class JsonValueScope;
class JsonArrayScope;
class JsonObjectScope;

class JsonBuilder {
 public:
  explicit JsonBuilder(StringBuilder &&sb) : sb_(std::move(sb)) {
  }
  StringBuilder &string_builder() {
    return sb_;
  }
  friend class JsonScope;
  JsonValueScope enter_value() WARN_UNUSED_RESULT;
  JsonArrayScope enter_array() WARN_UNUSED_RESULT;
  JsonObjectScope enter_object() WARN_UNUSED_RESULT;

 private:
  StringBuilder sb_;
  JsonScope *scope_ = nullptr;
};

class Jsonable {};

class JsonScope {
 public:
  explicit JsonScope(JsonBuilder *jb) : sb_(&jb->sb_), jb_(jb) {
    save_scope_ = jb_->scope_;
    jb_->scope_ = this;
    CHECK(is_active());
  }
  JsonScope(const JsonScope &other) = delete;
  JsonScope(JsonScope &&other) : sb_(other.sb_), jb_(other.jb_), save_scope_(other.save_scope_) {
    other.jb_ = nullptr;
  }
  JsonScope &operator=(const JsonScope &) = delete;
  JsonScope &operator=(JsonScope &&) = delete;
  ~JsonScope() {
    if (jb_) {
      exit();
    }
  }
  void exit() {
    CHECK(is_active());
    jb_->scope_ = save_scope_;
  }

 protected:
  StringBuilder *sb_;

  // For CHECK
  JsonBuilder *jb_;
  JsonScope *save_scope_;

  bool is_active() {
    return jb_->scope_ == this;
  }

  JsonScope &operator<<(JsonTrue x) {
    *sb_ << x;
    return *this;
  }
  JsonScope &operator<<(JsonFalse x) {
    *sb_ << x;
    return *this;
  }
  JsonScope &operator<<(const JsonBool &x) {
    *sb_ << x;
    return *this;
  }
  JsonScope &operator<<(const JsonInt &x) {
    *sb_ << x;
    return *this;
  }
  JsonScope &operator<<(const JsonLong &x) {
    *sb_ << x;
    return *this;
  }
  JsonScope &operator<<(const JsonFloat &x) {
    *sb_ << x;
    return *this;
  }
  JsonScope &operator<<(const JsonString &x) {
    *sb_ << x;
    return *this;
  }
  JsonScope &operator<<(const JsonRawString &x) {
    *sb_ << x;
    return *this;
  }
  JsonScope &operator<<(const JsonRaw &x) {
    *sb_ << x;
    return *this;
  }
  JsonScope &operator<<(bool x) {
    return *this << JsonBool(x);
  }
  JsonScope &operator<<(int32 x) {
    return *this << JsonInt(x);
  }
  JsonScope &operator<<(int64 x) {
    return *this << JsonLong(x);
  }
  JsonScope &operator<<(double x) {
    return *this << JsonFloat(x);
  }
  template <class T>
  JsonScope &operator<<(const T *x);  // not implemented
  template <size_t N>
  JsonScope &operator<<(const char (&x)[N]) {
    return *this << JsonString(Slice(x));
  }
  JsonScope &operator<<(const char *x) {
    return *this << JsonString(Slice(x));
  }
  JsonScope &operator<<(const string &x) {
    return *this << JsonString(Slice(x));
  }
  JsonScope &operator<<(Slice x) {
    return *this << JsonString(x);
  }
};

class JsonValueScope : public JsonScope {
 public:
  using JsonScope::JsonScope;
  template <class T>
  std::enable_if_t<std::is_base_of<Jsonable, typename std::decay<T>::type>::value, JsonValueScope &> operator<<(
      const T &x) {
    x.store(this);
    return *this;
  }
  template <class T>
  std::enable_if_t<!std::is_base_of<Jsonable, typename std::decay<T>::type>::value, JsonValueScope &> operator<<(
      const T &x) {
    CHECK(!was_);
    was_ = true;
    JsonScope::operator<<(x);
    return *this;
  }

  JsonArrayScope enter_array() WARN_UNUSED_RESULT;
  JsonObjectScope enter_object() WARN_UNUSED_RESULT;

 private:
  bool was_ = false;
};

class JsonArrayScope : public JsonScope {
 public:
  explicit JsonArrayScope(JsonBuilder *jb) : JsonScope(jb) {
    *sb_ << "[";
  }
  JsonArrayScope(JsonArrayScope &&other) = default;
  ~JsonArrayScope() {
    if (jb_) {
      exit();
    }
  }
  void exit() {
    *sb_ << "]";
  }
  template <class T>
  JsonArrayScope &operator<<(const T &x) {
    enter_value() << x;
    return *this;
  }
  JsonValueScope enter_value() {
    CHECK(is_active());
    if (is_first_) {
      *sb_ << ",";
    } else {
      is_first_ = true;
    }
    return jb_->enter_value();
  }

 private:
  bool is_first_ = false;
};

class JsonObjectScope : public JsonScope {
 public:
  explicit JsonObjectScope(JsonBuilder *jb) : JsonScope(jb) {
    *sb_ << "{";
  }
  JsonObjectScope(JsonObjectScope &&other) = default;
  ~JsonObjectScope() {
    if (jb_) {
      exit();
    }
  }
  void exit() {
    *sb_ << "}";
  }
  template <class S, class T>
  JsonObjectScope &operator<<(std::tuple<S, T> key_value) {
    return *this << std::pair<S, T>(std::get<0>(key_value), std::get<1>(key_value));
  }
  template <class S, class T>
  JsonObjectScope &operator<<(std::pair<S, T> key_value) {
    CHECK(is_active());
    if (is_first_) {
      *sb_ << ",";
    } else {
      is_first_ = true;
    }
    jb_->enter_value() << key_value.first;
    *sb_ << ":";
    jb_->enter_value() << key_value.second;
    return *this;
  }
  JsonObjectScope &operator<<(const JsonRaw &key_value) {
    CHECK(is_active());
    is_first_ = true;
    jb_->enter_value() << key_value;
    return *this;
  }

 private:
  bool is_first_ = false;
};

inline JsonArrayScope JsonValueScope::enter_array() {
  CHECK(!was_);
  was_ = true;
  return JsonArrayScope(jb_);
}
inline JsonObjectScope JsonValueScope::enter_object() {
  CHECK(!was_);
  was_ = true;
  return JsonObjectScope(jb_);
}
inline JsonValueScope JsonBuilder::enter_value() {
  return JsonValueScope(this);
}
inline JsonObjectScope JsonBuilder::enter_object() {
  return JsonObjectScope(this);
}
inline JsonArrayScope JsonBuilder::enter_array() {
  return JsonArrayScope(this);
}

class JsonValue;

using JsonObject = vector<std::pair<MutableSlice, JsonValue>>;
using JsonArray = vector<JsonValue>;

class JsonValue : public Jsonable {
 public:
  enum class Type { Null, Number, Boolean, String, Array, Object };

  static Slice get_type_name(Type type);

  JsonValue() : type_(Type::Null) {
  }
  ~JsonValue() {
    destroy();
  }
  JsonValue(JsonValue &&other) : JsonValue() {
    init(std::move(other));
  }
  JsonValue &operator=(JsonValue &&other) {
    if (&other == this) {
      return *this;
    }
    destroy();
    init(std::move(other));
    return *this;
  }
  JsonValue(const JsonValue &other) = delete;
  JsonValue &operator=(const JsonValue &other) = delete;

  Type type() const {
    return type_;
  }

  MutableSlice &get_string() {
    CHECK(type_ == Type::String);
    return string_;
  }
  const MutableSlice &get_string() const {
    CHECK(type_ == Type::String);
    return string_;
  }
  bool &get_boolean() {
    CHECK(type_ == Type::Boolean);
    return boolean_;
  }
  const bool &get_boolean() const {
    CHECK(type_ == Type::Boolean);
    return boolean_;
  }

  MutableSlice &get_number() {
    CHECK(type_ == Type::Number);
    return number_;
  }
  const MutableSlice &get_number() const {
    CHECK(type_ == Type::Number);
    return number_;
  }

  JsonArray &get_array() {
    CHECK(type_ == Type::Array);
    return array_;
  }
  const JsonArray &get_array() const {
    CHECK(type_ == Type::Array);
    return array_;
  }

  JsonObject &get_object() {
    CHECK(type_ == Type::Object);
    return object_;
  }
  const JsonObject &get_object() const {
    CHECK(type_ == Type::Object);
    return object_;
  }

  static JsonValue create_boolean(bool val) {
    JsonValue res;
    res.init_boolean(val);
    return res;
  }

  static JsonValue create_number(MutableSlice number) {
    JsonValue res;
    res.init_number(number);
    return res;
  }

  static JsonValue create_string(MutableSlice str) {
    JsonValue res;
    res.init_string(str);
    return res;
  }

  static JsonValue create_array(JsonArray v) {
    JsonValue res;
    res.init_array(std::move(v));
    return res;
  }

  static JsonValue create_object(JsonObject c) {
    JsonValue res;
    res.init_object(std::move(c));
    return res;
  }

  void store(JsonValueScope *scope) const {
    switch (type_) {
      case Type::Null:
        *scope << JsonRaw("null");
        break;
      case Type::Boolean:
        if (get_boolean()) {
          *scope << JsonRaw("true");
        } else {
          *scope << JsonRaw("false");
        }
        break;
      case Type::Number:
        *scope << JsonRaw(get_number());
        break;
      case Type::String:
        *scope << JsonString(get_string());
        break;
      case Type::Array: {
        auto arr = scope->enter_array();
        for (auto &val : get_array()) {
          arr << val;
        }
        break;
      }
      case Type::Object: {
        auto object = scope->enter_object();
        for (auto &key_value : get_object()) {
          object << ctie(JsonString(key_value.first), key_value.second);
        }
        break;
      }
    }
  };

 private:
  Type type_;
  union {
    MutableSlice number_;
    bool boolean_;
    MutableSlice string_;
    JsonArray array_;
    JsonObject object_;
  };

  void init_null() {
    type_ = Type::Null;
  }
  void init_number(MutableSlice number) {
    type_ = Type::Number;
    new (&number_) MutableSlice(number);
  }
  void init_boolean(bool boolean) {
    type_ = Type::Boolean;
    boolean_ = boolean;
  }
  void init_string(MutableSlice slice) {
    type_ = Type::String;
    new (&string_) MutableSlice(slice);
  }
  void init_array(JsonArray array) {
    type_ = Type::Array;
    new (&array_) JsonArray(std::move(array));
  }
  void init_object(JsonObject object) {
    type_ = Type::Object;
    new (&object_) JsonObject(std::move(object));
  }

  void init(JsonValue &&other) {
    switch (other.type_) {
      case Type::Null:
        break;
      case Type::Number:
        init_number(other.number_);
        break;
      case Type::Boolean:
        init_boolean(other.boolean_);
        break;
      case Type::String:
        init_string(other.string_);
        break;
      case Type::Array:
        init_array(std::move(other.array_));
        break;
      case Type::Object:
        init_object(std::move(other.object_));
        break;
    }
    other.destroy();
  }

  void destroy() {
    switch (type_) {
      case Type::Null:
      case Type::Boolean:
        break;
      case Type::Number:
        number_.~MutableSlice();
        break;
      case Type::String:
        string_.~MutableSlice();
        break;
      case Type::Array:
        array_.~vector<JsonValue>();
        break;
      case Type::Object:
        object_.~vector<std::pair<MutableSlice, JsonValue>>();
        break;
    }
    type_ = Type::Null;
  }
};

class VirtuallyJsonable : public Jsonable {
 public:
  virtual void store(JsonValueScope *scope) const = 0;
  VirtuallyJsonable() = default;
  VirtuallyJsonable(const VirtuallyJsonable &) = delete;
  VirtuallyJsonable &operator=(const VirtuallyJsonable &) = delete;
  virtual ~VirtuallyJsonable() = default;
};

class VirtuallyJsonableInt : public VirtuallyJsonable {
 public:
  explicit VirtuallyJsonableInt(int32 value) : value_(value) {
  }
  void store(JsonValueScope *scope) const override {
    *scope << JsonInt(value_);
  }

 private:
  int32 value_;
};

class VirtuallyJsonableLong : public VirtuallyJsonable {
 public:
  explicit VirtuallyJsonableLong(int64 value) : value_(value) {
  }
  void store(JsonValueScope *scope) const override {
    *scope << JsonLong(value_);
  }

 private:
  int64 value_;
};

Result<MutableSlice> json_string_decode(Parser &parser) WARN_UNUSED_RESULT;
Status json_string_skip(Parser &parser) WARN_UNUSED_RESULT;
Result<JsonValue> do_json_decode(Parser &parser) WARN_UNUSED_RESULT;
Status do_json_skip(Parser &parser) WARN_UNUSED_RESULT;

inline Result<JsonValue> json_decode(MutableSlice from) {
  Parser parser(from);
  auto result = do_json_decode(parser);
  if (result.is_ok() && !parser.empty()) {
    return Status::Error("Expected string end");
  }
  return result;
}

template <class StrT, class ValT>
StrT json_encode(const ValT &val) {
  auto buf_len = 1 << 19;
  auto buf = StackAllocator<>::alloc(buf_len);
  JsonBuilder jb(StringBuilder(buf.as_slice()));
  jb.enter_value() << val;
  LOG_IF(ERROR, jb.string_builder().is_error()) << "Json buffer overflow";
  auto slice = jb.string_builder().as_cslice();
  return StrT(slice.begin(), slice.size());
}

bool has_json_object_field(JsonObject &object, Slice name);

Result<JsonValue> get_json_object_field(JsonObject &object, Slice name, JsonValue::Type type,
                                        bool is_optional = true) WARN_UNUSED_RESULT;

Result<bool> get_json_object_bool_field(JsonObject &object, Slice name, bool is_optional = true,
                                        bool default_value = false) WARN_UNUSED_RESULT;

Result<int32> get_json_object_int_field(JsonObject &object, Slice name, bool is_optional = true,
                                        int32 default_value = 0) WARN_UNUSED_RESULT;

Result<double> get_json_object_double_field(JsonObject &object, Slice name, bool is_optional = true,
                                            double default_value = 0.0) WARN_UNUSED_RESULT;

Result<string> get_json_object_string_field(JsonObject &object, Slice name, bool is_optional = true,
                                            string default_value = "") WARN_UNUSED_RESULT;

}  // namespace td
