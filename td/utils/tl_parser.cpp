#include <cassert>

#include "tl_parser.h"

namespace td {
namespace tl {

const int32_t tl_parser::empty_data[8] = {0, 0, 0, 0, 0, 0, 0, 0};

void tl_parser::set_error(const string &error_message) {
  if (error.empty()) {
    CHECK(!error_message.empty());
    error = error_message;
    error_pos = get_pos();
    data = empty_data;
    data_begin = empty_data;
    data_len = 0;
  } else {
    data = empty_data;
    assert(error_pos >= 0);
    assert(data_len == 0);
  }
}

}  // namespace tl
}  // namespace td
