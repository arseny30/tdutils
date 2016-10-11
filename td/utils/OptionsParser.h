#pragma once

#include "td/utils/common.h"
#include "td/utils/Slice.h"
#include "td/utils/StringBuilder.h"
#include "td/utils/Status.h"

#include <functional>
#include <string>
#include <getopt.h>

namespace td {
class OptionsParser {
 public:
  class Option {
   public:
    enum Type { NoArg, Arg, OptionalArg };
    Type type;
    char short_key;
    std::string long_key;
    std::string description;
    std::function<Status(Slice)> arg_callback;
  };

  void set_description(std::string description) {
    description_ = description;
  }

  void add_option(Option::Type type, char short_key, Slice long_key, Slice description,
                  std::function<Status(Slice)> callback) {
    options_.push_back(Option{type, short_key, long_key.str(), description.str(), std::move(callback)});
  }

  void add_option(char short_key, Slice long_key, Slice description, std::function<Status(Slice)> callback) {
    add_option(Option::Type::Arg, short_key, long_key, description, std::move(callback));
  }

  void add_option(char short_key, Slice long_key, Slice description, std::function<Status(void)> callback) {
    // Ouch. There must be some better way
    add_option(Option::Type::NoArg, short_key, long_key, description,
               std::bind([](std::function<Status(void)> &func, Slice) { return func(); }, std::move(callback),
                         std::placeholders::_1));
  }

  Result<int> run(int argc, char *argv[]) WARN_UNUSED_RESULT {
    // use getopt. long keys are not supported for now
    char buff[1024];
    StringBuilder sb(buff);
    for (auto &opt : options_) {
      CHECK(opt.type != Option::OptionalArg);
      sb << opt.short_key;
      if (opt.type == Option::Arg) {
        sb << ":";
      }
    }
    if (sb.is_error()) {
      return Status::Error("Can't parse options");
    }
    CSlice short_options = sb.as_cslice();

    vector<struct option> long_options;
    for (auto &opt : options_) {
      if (opt.long_key.empty()) {
        continue;
      }
      struct option o;
      o.flag = nullptr;
      o.val = opt.short_key;
      o.has_arg = opt.type == Option::Arg ? required_argument : no_argument;
      o.name = opt.long_key.c_str();
      long_options.push_back(o);
    }
    long_options.push_back({nullptr, 0, nullptr, 0});

    while (true) {
      int opt_i = getopt_long(argc, argv, short_options.c_str(), &long_options[0], nullptr);
      if (opt_i == ':') {
        return Status::Error("Missing argument");
      }
      if (opt_i == '?') {
        return Status::Error("Unrecognized option");
      }
      if (opt_i == -1) {
        break;
      }
      bool found = false;
      for (auto &opt : options_) {
        if (opt.short_key == opt_i) {
          Slice arg;
          if (opt.type == Option::Arg) {
            arg = Slice(optarg);
          }
          auto status = opt.arg_callback(arg);
          if (status.is_error()) {
            return std::move(status);
          }
          found = true;
          break;
        }
      }
      if (!found) {
        return Status::Error("Unknown argument");
      }
    }
    return optind;
  }

  friend StringBuilder &operator<<(StringBuilder &sb, const OptionsParser &o) {
    sb << o.description_ << "\n";
    for (auto &opt : o.options_) {
      sb << "-" << opt.short_key;
      if (!opt.long_key.empty()) {
        sb << "|--" << opt.long_key;
      }
      if (opt.type == Option::OptionalArg) {
        sb << "[";
      }
      if (opt.type != Option::NoArg) {
        sb << "<arg>";
      }
      if (opt.type == Option::OptionalArg) {
        sb << "]";
      }
      sb << "\t" << opt.description;
      sb << "\n";
    }
    return sb;
  }

 private:
  std::vector<Option> options_;
  std::string description_;
};
}  // end of namespace td
