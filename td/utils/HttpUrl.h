#pragma once

#include "td/utils/common.h"
#include "td/utils/Slice.h"
#include "td/utils/Status.h"
#include "td/utils/StringBuilder.h"

namespace td {

class HttpUrl {
 public:
  enum class Protocol { HTTP, HTTPS } protocol_ = Protocol::HTTP;
  string userinfo_;
  string host_;
  bool is_ipv6_ = false;
  int specified_port_ = 0;
  int port_ = 0;
  string query_;

  string get_url() const;
};

// TODO Slice instead of MutableSlice
Result<HttpUrl> parse_url(MutableSlice url,
                          HttpUrl::Protocol default_protocol = HttpUrl::Protocol::HTTP) TD_WARN_UNUSED_RESULT;

StringBuilder &operator<<(StringBuilder &sb, const HttpUrl &url);

string get_url_query_file_name(const string &query);

string get_url_file_name(const string &url);

}  // namespace td
