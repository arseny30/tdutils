#include "tl_parsers.h"

namespace td {
namespace tl {

const unsigned char tl_parser::empty_data[sizeof(UInt256)] = {};  // static zero-initialized

void tl_parser::set_error(const string &error_message) {
  if (error.empty()) {
    CHECK(!error_message.empty());
    error = error_message;
    error_pos = data_len - left_len;
    data = empty_data;
    left_len = 0;
    data_len = 0;
  } else {
    data = empty_data;
    CHECK(error_pos != std::numeric_limits<size_t>::max());
    CHECK(data_len == 0);
    CHECK(left_len == 0);
  }
}

}  // namespace tl
}  // namespace td
