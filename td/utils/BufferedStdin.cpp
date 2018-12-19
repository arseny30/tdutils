#include "td/utils/BufferedStdin.h"
#include "td/utils/buffer.h"
#include "td/utils/port/detail/PollableFd.h"

namespace td {
class BufferedStdin {
 public:
 private:
  PollableFdInfo info_;
  ChainBufferWriter writer_;
  ChainBufferReader reader_ = writer_.extract_reader();
};
}  // namespace td

