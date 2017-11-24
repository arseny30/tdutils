#pragma once

#include "td/utils/port/config.h"

#include "td/utils/common.h"
#include "td/utils/Slice.h"
#include "td/utils/Status.h"
#include "td/utils/StringBuilder.h"

#if !TD_WINDOWS
#include <arpa/inet.h>
#include <sys/socket.h>
#endif

namespace td {
class SocketFd;
class IPAddress {
 public:
  IPAddress();

  bool is_valid() const;

  const sockaddr *get_sockaddr() const;
  size_t get_sockaddr_len() const;
  int get_address_family() const;
  Slice get_ip_str() const;
  bool is_ipv4() const;
  uint32 get_ipv4() const;
  Slice get_ipv6() const;
  int get_port() const;
  void set_port(int port);

  IPAddress get_any_addr() const;

  Status init_ipv6_port(CSlice ipv6, int port) WARN_UNUSED_RESULT;
  Status init_ipv6_as_ipv4_port(CSlice ipv4, int port) WARN_UNUSED_RESULT;
  Status init_ipv4_port(CSlice ipv4, int port) WARN_UNUSED_RESULT;
  Status init_host_port(CSlice host, int port) WARN_UNUSED_RESULT;
  Status init_host_port(CSlice host, CSlice port) WARN_UNUSED_RESULT;
  Status init_host_port(CSlice host_port) WARN_UNUSED_RESULT;
  Status init_socket_address(const SocketFd &socket_fd) WARN_UNUSED_RESULT;
  Status init_peer_address(const SocketFd &socket_fd) WARN_UNUSED_RESULT;

  friend bool operator==(const IPAddress &a, const IPAddress &b);
  friend bool operator<(const IPAddress &a, const IPAddress &b);

  static CSlice ipv4_to_str(int32 ipv4);

 private:
  union {
    sockaddr_storage addr_;
    sockaddr sockaddr_;
    sockaddr_in ipv4_addr_;
    sockaddr_in6 ipv6_addr_;
  };
  bool is_valid_;

  Status init_sockaddr(sockaddr *addr, socklen_t len) WARN_UNUSED_RESULT;
  void init_ipv4_any();
  void init_ipv6_any();
};

StringBuilder &operator<<(StringBuilder &builder, const IPAddress &address);

}  // namespace td
