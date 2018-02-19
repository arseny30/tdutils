#pragma once

#include "td/utils/common.h"
#include "td/utils/Slice.h"
#include "td/utils/Status.h"
#include "td/utils/StringBuilder.h"

namespace td {

class HttpUrl {
 public:
  enum class Protocol { HTTP, HTTPS } protocol_;
  string userinfo_;
  string host_;
  bool is_ipv6;
  int specified_port_;
  int port_;
  string query_;

  string get_url() const;
};

// TODO Slice instead of MutableSlice
Result<HttpUrl> parse_url(MutableSlice url,
                          HttpUrl::Protocol default_protocol = HttpUrl::Protocol::HTTP) TD_WARN_UNUSED_RESULT;

StringBuilder &operator<<(StringBuilder &sb, const HttpUrl &url);

string get_url_query_file_name(const string &query);

}  // namespace td
