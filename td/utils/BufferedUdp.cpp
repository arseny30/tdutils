#include "td/utils/BufferedUdp.h"

namespace td {
#if TD_PORT_POSIX
TD_THREAD_LOCAL detail::UdpReader* BufferedUdp::udp_reader_;
#endif
}  // namespace td
