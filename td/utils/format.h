#pragma once
#include "td/utils/common.h"
#include "StringBuilder.h"

namespace td {
namespace format {
/*** HexDump ***/
template <int size, bool reversed = true>
struct HexDumpSize {
  const uint8 *data;
};

inline char hex_digit(int x) {
  return "0123456789abcdef"[x];
}

template <int size, bool reversed>
inline StringBuilder &operator<<(StringBuilder &builder, const HexDumpSize<size, reversed> &dump) {
  const uint8 *ptr = dump.data;
  // TODO: append unsafe
  if (!reversed) {
    for (int i = 0; i < size; i++) {
      int xy = ptr[i];
      int x = xy >> 4;
      int y = xy & 15;
      builder << hex_digit(x) << hex_digit(y);
    }
  } else {
    for (int i = size - 1; i >= 0; i--) {
      int xy = ptr[i];
      int x = xy >> 4;
      int y = xy & 15;
      builder << hex_digit(x) << hex_digit(y);
    }
  }
  return builder;
}

template <int align>
struct HexDumpSlice {
  Slice slice;
};

template <int align>
inline StringBuilder &operator<<(StringBuilder &builder, const HexDumpSlice<align> &dump) {
  int size = static_cast<int>(dump.slice.size());
  const uint8 *ptr = dump.slice.ubegin();

  builder << '\n';

  if (align == 0) {
    for (int i = 0; i < size; i++) {
      builder << HexDumpSize<1>{ptr};
    }
    return builder;
  }

  const int part = size % align;
  if (part) {
    builder << HexDumpSlice<1>{Slice(ptr, part)} << '\n';
  }
  size -= part;
  ptr += part;

  for (int i = 0; i < size; i += align) {
    builder << HexDumpSize<align>{ptr};
    ptr += align;

    if (((i / align) & 15) == 15 || i + align == size) {
      builder << '\n';
    } else {
      builder << ' ';
    }
  }

  return builder;
}

template <int align>
inline HexDumpSlice<align> as_hex_dump(const Slice &slice) {
  return HexDumpSlice<align>{slice};
}

template <int align>
inline HexDumpSlice<align> as_hex_dump(const MutableSlice &slice) {
  return HexDumpSlice<align>{slice};
}

template <int align, class T>
inline HexDumpSlice<align> as_hex_dump(const T &value) {
  return HexDumpSlice<align>{Slice(&value, sizeof(value))};
}
template <class T>
inline HexDumpSize<sizeof(T), true> as_hex_dump(const T &value) {
  return HexDumpSize<sizeof(T), true>{reinterpret_cast<const uint8 *>(&value)};
}

/*** Hex ***/
template <class T>
struct Hex {
  const T &value;
};

template <class T>
Hex<T> as_hex(const T &value) {
  return Hex<T>{value};
}

template <class T>
StringBuilder &operator<<(StringBuilder &builder, const Hex<T> &hex) {
  builder << "0x" << as_hex_dump(hex.value);
  return builder;
}

/*** Binary ***/
template <class T>
struct Binary {
  const T &value;
};

template <class T>
Binary<T> as_binary(const T &value) {
  return Binary<T>{value};
}

template <class T>
StringBuilder &operator<<(StringBuilder &builder, const Binary<T> &hex) {
  for (size_t i = 0; i < sizeof(T) * 8; i++) {
    builder << ((hex.value >> i) & 1 ? '1' : '0');
  }
  return builder;
}

/*** Escaped ***/
struct Escaped {
  Slice str;
};

inline StringBuilder &operator<<(StringBuilder &builder, const Escaped &escaped) {
  Slice str = escaped.str;
  for (size_t i = 0; i < str.size(); i++) {
    unsigned char c = str[i];
    if (c > 31 && c < 127 && c != '"' && c != '\\') {
      builder << static_cast<char>(c);
    } else {
      builder.printf("\\%03o", c);
    }
  }
  return builder;
}

inline Escaped escaped(Slice slice) {
  return Escaped{slice};
}

/*** Time to string ***/
struct Time {
  double seconds_;
};

inline StringBuilder &operator<<(StringBuilder &logger, Time t) {
  struct NamedValue {
    const char *name;
    double value;
  };

  static constexpr NamedValue durations[] = {{"ns", 1e-9}, {"us", 1e-6}, {"ms", 1e-3}, {"s", 1}};
  static constexpr size_t durations_n = sizeof(durations) / sizeof(NamedValue);

  size_t i = 0;
  while (i + 1 < durations_n && t.seconds_ > 10 * durations[i + 1].value) {
    i++;
  }
  logger.printf("%.1lf%s", t.seconds_ / durations[i].value, durations[i].name);
  return logger;
}

inline Time as_time(double seconds) {
  return Time{seconds};
}

/*** Size to string ***/
struct Size {
  uint64 size_;
};

inline StringBuilder &operator<<(StringBuilder &logger, Size t) {
  struct NamedValue {
    const char *name;
    uint64 value;
  };

  static constexpr NamedValue sizes[] = {{"B", 1}, {"KB", 1 << 10}, {"MB", 1 << 20}, {"GB", 1 << 30}};
  static constexpr size_t sizes_n = sizeof(sizes) / sizeof(NamedValue);

  size_t i = 0;
  while (i + 1 < sizes_n && t.size_ > 10 * sizes[i + 1].value) {
    i++;
  }
  logger << t.size_ / sizes[i].value << sizes[i].name;
  return logger;
}

inline Size as_size(uint64 size) {
  return Size{size};
}

/*** Array to string ***/
template <class ArrayT>
struct Array {
  const ArrayT &ref;
};

template <class ArrayT>
StringBuilder &operator<<(StringBuilder &stream, const Array<ArrayT> &array) {
  bool first = true;
  stream << Slice("{");
  for (auto &x : array.ref) {
    if (!first) {
      stream << Slice(", ");
    }
    stream << x;
    first = false;
  }
  return stream << Slice("}");
}

template <class ArrayT>
Array<ArrayT> as_array(const ArrayT &array) {
  return Array<ArrayT>{array};
}

/*** Tagged ***/
template <class ValueT>
struct Tagged {
  Slice tag;
  const ValueT &ref;
};

template <class ValueT>
StringBuilder &operator<<(StringBuilder &stream, const Tagged<ValueT> &tagged) {
  return stream << "[" << tagged.tag << ":" << tagged.ref << "]";
}

template <class ValueT>
Tagged<ValueT> tag(Slice tag, const ValueT &ref) {
  return Tagged<ValueT>{tag, ref};
}

/*** Cond ***/
inline StringBuilder &operator<<(StringBuilder &sb, Unit) {
  return sb;
}

template <class TrueT, class FalseT>
struct Cond {
  bool flag;
  const TrueT &on_true;
  const FalseT &on_false;
};

template <class TrueT, class FalseT>
inline StringBuilder &operator<<(StringBuilder &sb, const Cond<TrueT, FalseT> &cond) {
  if (cond.flag) {
    return sb << cond.on_true;
  } else {
    return sb << cond.on_false;
  }
}

template <class TrueT, class FalseT = Unit>
inline Cond<TrueT, FalseT> cond(bool flag, const TrueT &on_true, const FalseT &on_false = FalseT()) {
  return Cond<TrueT, FalseT>{flag, on_true, on_false};
}

/*** Concat ***/
template <class T>
struct Concat {
  T args;
};

template <class T>
inline StringBuilder &operator<<(StringBuilder &sb, const Concat<T> &concat) {
  tuple_for_each(concat.args, [&sb](auto &x) { sb << x; });
  return sb;
}

template <class... ArgsT>
inline auto concat(const ArgsT &... args) {
  return Concat<decltype(std::tie(args...))>{std::tie(args...)};
}

}  // end of namespace format
using format::tag;

template <class A, class B>
StringBuilder &operator<<(StringBuilder &sb, std::pair<A, B> p) {
  return sb << "[" << p.first << ";" << p.second << "]";
}

}  // end of namespace td
