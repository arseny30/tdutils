#pragma once

#include "td/utils/common.h"
#include "td/utils/Slice.h"
#include "td/utils/Status.h"

namespace td {

string base64_encode(Slice input);
Result<string> base64_decode(Slice input);

string base64url_encode(Slice input);
Result<string> base64url_decode(Slice base64);

}  // namespace td
