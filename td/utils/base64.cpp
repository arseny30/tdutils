#include "td/utils/base64.h"

#include <algorithm>
#include <iterator>

namespace td {
//TODO: fix copypaste

static const char *const symbols64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

string base64_encode(Slice input) {
  string base64;
  base64.reserve((input.size() + 2) / 3 * 4);
  for (size_t i = 0; i < input.size();) {
    size_t left = std::min(input.size() - i, static_cast<size_t>(3));
    size_t c = input.ubegin()[i++] << 16;
    base64 += symbols64[c >> 18];
    if (left != 1) {
      c |= input.ubegin()[i++] << 8;
    }
    base64 += symbols64[(c >> 12) & 63];
    if (left == 3) {
      c |= input.ubegin()[i++];
    }
    if (left != 1) {
      base64 += symbols64[(c >> 6) & 63];
    } else {
      base64 += '=';
    }
    if (left == 3) {
      base64 += symbols64[c & 63];
    } else {
      base64 += '=';
    }
  }
  return base64;
}

static unsigned char char_to_value[256];
static bool init_char_to_value() {
  std::fill(std::begin(char_to_value), std::end(char_to_value), 64);
  for (unsigned char i = 0; i < 64; i++) {
    char_to_value[static_cast<size_t>(symbols64[i])] = i;
  }
  return true;
}

Result<string> base64_decode(Slice base64) {
  static bool is_inited = init_char_to_value();
  CHECK(is_inited);

  if ((base64.size() & 3) != 0) {
    return Status::Error("Wrong string length");
  }

  size_t padding_length = 0;
  while (base64.size() > 0 && base64.back() == '=') {
    base64.remove_suffix(1);
    padding_length++;
  }
  if (padding_length >= 3) {
    return Status::Error("Wrong string padding");
  }

  string output;
  output.reserve(((base64.size() + 3) >> 2) * 3);
  for (size_t i = 0; i < base64.size();) {
    size_t left = std::min(base64.size() - i, static_cast<size_t>(4));
    size_t c = 0;
    for (size_t t = 0; t < left; t++) {
      auto value = char_to_value[base64.ubegin()[i++]];
      if (value == 64) {
        return Status::Error("Wrong character in the string");
      }
      c |= value << ((3 - t) * 6);
    }
    output += static_cast<char>(static_cast<unsigned char>(c >> 16));  // implementation-defined
    if (left == 2) {
      if ((c & ((1 << 16) - 1)) != 0) {
        return Status::Error("Wrong padding in the string");
      }
    } else {
      output += static_cast<char>(static_cast<unsigned char>(c >> 8));  // implementation-defined
      if (left == 3) {
        if ((c & ((1 << 8) - 1)) != 0) {
          return Status::Error("Wrong padding in the string");
        }
      } else {
        output += static_cast<char>(static_cast<unsigned char>(c));  // implementation-defined
      }
    }
  }
  return output;
}

static const char *const url_symbols64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

string base64url_encode(Slice input) {
  string base64;
  base64.reserve((input.size() + 2) / 3 * 4);
  for (size_t i = 0; i < input.size();) {
    size_t left = std::min(input.size() - i, static_cast<size_t>(3));
    size_t c = input.ubegin()[i++] << 16;
    base64 += url_symbols64[c >> 18];
    if (left != 1) {
      c |= input.ubegin()[i++] << 8;
    }
    base64 += url_symbols64[(c >> 12) & 63];
    if (left == 3) {
      c |= input.ubegin()[i++];
    }
    if (left != 1) {
      base64 += url_symbols64[(c >> 6) & 63];
    }
    if (left == 3) {
      base64 += url_symbols64[c & 63];
    }
  }
  return base64;
}

static unsigned char url_char_to_value[256];
static bool init_url_char_to_value() {
  std::fill(std::begin(url_char_to_value), std::end(url_char_to_value), 64);
  for (unsigned char i = 0; i < 64; i++) {
    url_char_to_value[static_cast<size_t>(url_symbols64[i])] = i;
  }
  return true;
}

Result<string> base64url_decode(Slice base64) {
  static bool is_inited = init_url_char_to_value();
  CHECK(is_inited);

  size_t padding_length = 0;
  while (base64.size() > 0 && base64.back() == '=') {
    base64.remove_suffix(1);
    padding_length++;
  }
  if (padding_length >= 3 || (padding_length > 0 && ((base64.size() + padding_length) & 3) != 0)) {
    return Status::Error("Wrong string padding");
  }

  if ((base64.size() & 3) == 1) {
    return Status::Error("Wrong string length");
  }

  string output;
  output.reserve(((base64.size() + 3) >> 2) * 3);
  for (size_t i = 0; i < base64.size();) {
    size_t left = std::min(base64.size() - i, static_cast<size_t>(4));
    size_t c = 0;
    for (size_t t = 0; t < left; t++) {
      auto value = url_char_to_value[base64.ubegin()[i++]];
      if (value == 64) {
        return Status::Error("Wrong character in the string");
      }
      c |= value << ((3 - t) * 6);
    }
    output += static_cast<char>(static_cast<unsigned char>(c >> 16));  // implementation-defined
    if (left == 2) {
      if ((c & ((1 << 16) - 1)) != 0) {
        return Status::Error("Wrong padding in the string");
      }
    } else {
      output += static_cast<char>(static_cast<unsigned char>(c >> 8));  // implementation-defined
      if (left == 3) {
        if ((c & ((1 << 8) - 1)) != 0) {
          return Status::Error("Wrong padding in the string");
        }
      } else {
        output += static_cast<char>(static_cast<unsigned char>(c));  // implementation-defined
      }
    }
  }
  return output;
}

}  // namespace td
