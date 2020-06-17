#pragma once

#include "td/utils/common.h"
#include "td/utils/Slice.h"
#include "td/utils/Status.h"
#include "td/utils/StringBuilder.h"

#include <functional>

namespace td {

class OptionParser {
  class Option {
   public:
    enum class Type { NoArg, Arg };
    Type type;
    char short_key;
    string long_key;
    string description;
    std::function<Status(Slice)> arg_callback;
  };

  void add_option(Option::Type type, char short_key, Slice long_key, Slice description,
                  std::function<Status(Slice)> callback);

 public:
  void set_description(string description);

  void add_option(char short_key, Slice long_key, Slice description, std::function<Status(Slice)> callback);

  void add_option(char short_key, Slice long_key, Slice description, std::function<Status(void)> callback);

  // returns found non-option parameters
  Result<vector<char *>> run(int argc, char *argv[]) TD_WARN_UNUSED_RESULT;

  friend StringBuilder &operator<<(StringBuilder &sb, const OptionParser &o);

 private:
  vector<Option> options_;
  string description_;
};

}  // namespace td
