#include "td/utils/port/config.h"

#include "td/utils/port/IPAddress.h"

#include "td/utils/ScopeGuard.h"
#include "td/utils/format.h"
#include "td/utils/misc.h"

#if !TD_WINDOWS
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#endif

#include <algorithm>

namespace td {
/*** IPAddress ***/
IPAddress::IPAddress() : is_valid_(false) {
}

bool IPAddress::is_valid() const {
  return is_valid_;
}

const struct sockaddr *IPAddress::get_sockaddr() const {
  return &sockaddr_;
}

size_t IPAddress::get_sockaddr_len() const {
  CHECK(is_valid());
  switch (addr_.ss_family) {
    case AF_INET6:
      return sizeof(ipv6_addr_);
    case AF_INET:
      return sizeof(ipv4_addr_);
    default:
      UNREACHABLE("Unknown address family");
  }
}

int IPAddress::get_address_family() const {
  return get_sockaddr()->sa_family;
}

bool IPAddress::is_ipv4() const {
  return get_address_family() == AF_INET;
}

uint32 IPAddress::get_ipv4() const {
  CHECK(is_valid());
  CHECK(get_address_family() == AF_INET);
  return ipv4_addr_.sin_addr.s_addr;
}

IPAddress IPAddress::get_any_addr() const {
  IPAddress res;
  switch (get_address_family()) {
    case AF_INET6:
      res.init_ipv6_any();
      break;
    case AF_INET:
      res.init_ipv4_any();
      break;
    default:
      UNREACHABLE("Unknown address family");
  }
  return res;
}
void IPAddress::init_ipv4_any() {
  is_valid_ = true;
  ipv4_addr_.sin_family = AF_INET;
  ipv4_addr_.sin_addr.s_addr = INADDR_ANY;
  ipv4_addr_.sin_port = 0;
}
void IPAddress::init_ipv6_any() {
  is_valid_ = true;
  ipv6_addr_.sin6_family = AF_INET6;
  ipv6_addr_.sin6_addr = in6addr_any;
  ipv6_addr_.sin6_port = 0;
}

Status IPAddress::init_ipv6_port(const CSlice &ipv6, int port) {
  is_valid_ = false;
  if (port <= 0 || port >= (1 << 16)) {
    return Status::Error(PSTR() << "Invalid [port=" << port << "]");
  }
  memset(&ipv6_addr_, 0, sizeof(ipv6_addr_));
  ipv6_addr_.sin6_family = AF_INET6;
  ipv6_addr_.sin6_port = htons(static_cast<uint16>(port));
  // NB: ipv6 must be zero terminated...
  int err = inet_pton(AF_INET6, ipv6.begin(), &ipv6_addr_.sin6_addr);
  if (err == 0) {
    return Status::Error(PSTR() << "Failed inet_pton(AF_INET6, " << ipv6 << ")");
  } else if (err == -1) {
    return Status::OsError(PSTR() << "Failed inet_pton(AF_INET6, " << ipv6 << ")");
  }
  is_valid_ = true;
  return Status::OK();
}

Status IPAddress::init_ipv6_as_ipv4_port(const CSlice &ipv4, int port) {
  return init_ipv6_port(string("::FFFF:").append(ipv4.begin(), ipv4.size()), port);
}

Status IPAddress::init_ipv4_port(const CSlice &ipv4, int port) {
  is_valid_ = false;
  if (port <= 0 || port >= (1 << 16)) {
    return Status::Error(PSTR() << "Invalid [port=" << port << "[");
  }
  memset(&ipv4_addr_, 0, sizeof(ipv4_addr_));
  ipv4_addr_.sin_family = AF_INET;
  ipv4_addr_.sin_port = htons(static_cast<uint16>(port));
  // NB: ipv4 must be zero terminated...
  int err = inet_pton(AF_INET, ipv4.begin(), &ipv4_addr_.sin_addr);
  if (err == 0) {
    return Status::Error(PSTR() << "Failed inet_pton(AF_INET, " << ipv4 << ")");
  } else if (err == -1) {
    return Status::OsError(PSTR() << "Failed inet_pton(AF_INET, " << ipv4 << ")");
  }
  is_valid_ = true;
  return Status::OK();
}

Status IPAddress::init_host_port(const CSlice &host, int port) {
  auto str_port = to_string(port);
  struct addrinfo hints;
  struct addrinfo *info = nullptr;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;  // TODO AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  LOG(INFO) << "Try to init ipv4 address with port " << str_port;
  auto s = getaddrinfo(host.c_str(), str_port.c_str(), &hints, &info);
  if (s != 0) {
    return Status::Error(PSTR() << "getaddrinfo: " << gai_strerror(s));
  }
  SCOPE_EXIT {
    freeaddrinfo(info);
  };

  // prefer ipv4
  struct addrinfo *best_info = info;
  for (auto *ptr = info->ai_next; ptr != nullptr; ptr = ptr->ai_next) {
    if (ptr->ai_socktype == AF_INET) {
      best_info = ptr;
      break;
    }
  }
  // just use first address
  CHECK(best_info != nullptr);
  return init_sockaddr(best_info->ai_addr, narrow_cast<socklen_t>(best_info->ai_addrlen));
}

Status IPAddress::init_sockaddr(struct sockaddr *addr, socklen_t len) {
  if (addr->sa_family == AF_INET6) {
    CHECK(len == sizeof(ipv6_addr_));
    memcpy(&ipv6_addr_, reinterpret_cast<struct sockaddr_in6 *>(addr), sizeof(ipv6_addr_));
  } else if (addr->sa_family == AF_INET) {
    CHECK(len == sizeof(ipv4_addr_));
    memcpy(&ipv4_addr_, reinterpret_cast<struct sockaddr_in *>(addr), sizeof(ipv4_addr_));
    LOG(INFO) << "Have ipv4 address with port " << ntohs(ipv4_addr_.sin_port);
  } else {
    return Status::Error(PSTR() << "Unknown " << tag("sa_family", addr->sa_family));
  }

  is_valid_ = true;
  return Status::OK();
}

Slice IPAddress::get_ip_str() const {
  if (!is_valid()) {
    return Slice("0.0.0.0");
  }

  const int buf_size = INET6_ADDRSTRLEN;  //, INET_ADDRSTRLEN;
  static TD_THREAD_LOCAL char buf[buf_size];
  const void *addr;
  switch (get_address_family()) {
    case AF_INET6:
      addr = &ipv6_addr_.sin6_addr;
      break;
    case AF_INET:
      addr = &ipv4_addr_.sin_addr;
      break;
    default:
      UNREACHABLE();
  }

  const char *res = inet_ntop(get_address_family(),
#if TD_WINDOWS
                              const_cast<PVOID>(addr),
#else
                              addr,
#endif
                              buf, buf_size);
  if (res == nullptr) {
    return Slice();
  } else {
    return Slice(res);
  }
}

int IPAddress::get_port() const {
  if (!is_valid()) {
    return 0;
  }

  switch (get_address_family()) {
    case AF_INET6:
      return ntohs(ipv6_addr_.sin6_port);
    case AF_INET:
      return ntohs(ipv4_addr_.sin_port);
    default:
      UNREACHABLE();
  }
}

void IPAddress::set_port(int port) {
  CHECK(is_valid());

  switch (get_address_family()) {
    case AF_INET6:
      ipv6_addr_.sin6_port = htons(static_cast<uint16>(port));
      break;
    case AF_INET:
      ipv4_addr_.sin_port = htons(static_cast<uint16>(port));
      break;
    default:
      UNREACHABLE();
  }
}

bool operator==(const IPAddress &a, const IPAddress &b) {
  if (!a.is_valid() || !b.is_valid()) {
    return false;
  }
  if (a.get_address_family() != b.get_address_family()) {
    return false;
  }

  if (a.get_address_family() == AF_INET) {
    return a.ipv4_addr_.sin_port == b.ipv4_addr_.sin_port &&
           memcmp(&a.ipv4_addr_.sin_addr, &b.ipv4_addr_.sin_addr, sizeof(a.ipv4_addr_.sin_addr)) == 0;
  } else if (a.get_address_family() == AF_INET6) {
    return a.ipv6_addr_.sin6_port == b.ipv6_addr_.sin6_port &&
           memcmp(&a.ipv6_addr_.sin6_addr, &b.ipv6_addr_.sin6_addr, sizeof(a.ipv6_addr_.sin6_addr)) == 0;
  }

  UNREACHABLE("Unknown address family");
}
bool operator<(const IPAddress &a, const IPAddress &b) {
  if (a.is_valid() != b.is_valid()) {
    return a.is_valid() < b.is_valid();
  }
  if (a.get_address_family() != b.get_address_family()) {
    return a.get_address_family() < b.get_address_family();
  }

  if (a.get_address_family() == AF_INET) {
    if (a.ipv4_addr_.sin_port != b.ipv4_addr_.sin_port) {
      return a.ipv4_addr_.sin_port < b.ipv4_addr_.sin_port;
    }
    return memcmp(&a.ipv4_addr_.sin_addr, &b.ipv4_addr_.sin_addr, sizeof(a.ipv4_addr_.sin_addr)) < 0;
  } else if (a.get_address_family() == AF_INET6) {
    if (a.ipv6_addr_.sin6_port != b.ipv6_addr_.sin6_port) {
      return a.ipv6_addr_.sin6_port < b.ipv6_addr_.sin6_port;
    }
    return memcmp(&a.ipv6_addr_.sin6_addr, &b.ipv6_addr_.sin6_addr, sizeof(a.ipv6_addr_.sin6_addr)) < 0;
  }

  UNREACHABLE("Unknown address family");
}

StringBuilder &operator<<(StringBuilder &builder, const IPAddress &address) {
  if (!address.is_valid()) {
    return builder << "[invalid]";
  }
  if (address.get_address_family() == AF_INET) {
    return builder << "[" << address.get_ip_str() << ":" << address.get_port() << "]";
  } else {
    CHECK(address.get_address_family() == AF_INET6);
    return builder << "[[" << address.get_ip_str() << "]:" << address.get_port() << "]";
  }
}

}  // namespace td
