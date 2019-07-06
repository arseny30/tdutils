#pragma once

#include "td/utils/port/config.h"

#include "td/utils/buffer.h"
#include "td/utils/optional.h"
#include "td/utils/port/detail/NativeFd.h"
#include "td/utils/port/detail/PollableFd.h"
#include "td/utils/port/IPAddress.h"
#include "td/utils/Slice.h"
#include "td/utils/Span.h"
#include "td/utils/Status.h"

#include <memory>

namespace td {
// Udp and errors
namespace detail {
class UdpSocketFdImpl;
class UdpSocketFdImplDeleter {
 public:
  void operator()(UdpSocketFdImpl *impl);
};
}  // namespace detail

struct UdpMessage {
  IPAddress address;
  BufferSlice data;
  Status error;
};

class UdpSocketFd {
 public:
  UdpSocketFd();
  UdpSocketFd(UdpSocketFd &&);
  UdpSocketFd &operator=(UdpSocketFd &&);
  ~UdpSocketFd();

  UdpSocketFd(const UdpSocketFd &) = delete;
  UdpSocketFd &operator=(const UdpSocketFd &) = delete;

  td::Result<td::uint32> maximize_snd_buffer(td::uint32 max_buffer_size = 0);
  td::Result<td::uint32> maximize_rcv_buffer(td::uint32 max_buffer_size = 0);

  static Result<UdpSocketFd> open(const IPAddress &address) TD_WARN_UNUSED_RESULT;

  PollableFdInfo &get_poll_info();
  const PollableFdInfo &get_poll_info() const;
  const NativeFd &get_native_fd() const;

  void close();
  bool empty() const;

  static bool is_critical_read_error(const Status &status);

#if TD_PORT_POSIX
  struct OutboundMessage {
    const IPAddress *to;
    Slice data;
  };
  struct InboundMessage {
    IPAddress *from;
    MutableSlice data;
    Status *error;
  };

  Status send_message(const OutboundMessage &message, bool &is_sent) TD_WARN_UNUSED_RESULT;
  Status receive_message(InboundMessage &message, bool &is_received) TD_WARN_UNUSED_RESULT;

  Status send_messages(Span<OutboundMessage> messages, size_t &count) TD_WARN_UNUSED_RESULT;
  Status receive_messages(MutableSpan<InboundMessage> messages, size_t &count) TD_WARN_UNUSED_RESULT;
#elif TD_PORT_WINDOWS
  Result<optional<UdpMessage> > receive();

  void send(UdpMessage message);

  Status flush_send();
#endif

 private:
  static constexpr td::uint32 default_udp_max_snd_buffer_size = (1 << 24);
  static constexpr td::uint32 default_udp_max_rcv_buffer_size = (1 << 24);
  std::unique_ptr<detail::UdpSocketFdImpl, detail::UdpSocketFdImplDeleter> impl_;
  explicit UdpSocketFd(unique_ptr<detail::UdpSocketFdImpl> impl);
};

}  // namespace td
