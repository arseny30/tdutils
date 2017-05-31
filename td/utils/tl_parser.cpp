#include "tl_parser.h"

namespace td {
namespace tl {

const int32 tl_parser::empty_data[8] = {0, 0, 0, 0, 0, 0, 0, 0};

void tl_parser::set_error(const string &error_message) {
  if (error.empty()) {
    CHECK(!error_message.empty());
    error = error_message;
    error_pos = static_cast<size_t>(data - data_begin);
    data = empty_data;
    data_begin = empty_data;
    data_len = 0;
  } else {
    data = empty_data;
    CHECK(error_pos != std::numeric_limits<size_t>::max());
    CHECK(data_len == 0);
  }
}

}  // namespace tl
}  // namespace td
