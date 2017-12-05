#pragma once

#include "td/utils/common.h"

#include <type_traits>

namespace td {

template <class FunctionT>
class member_function_class {
 private:
  template <class ResultT, class ClassT, class... ArgsT>
  static auto helper(ResultT (ClassT::*x)(ArgsT...)) {
    return static_cast<ClassT *>(nullptr);
  }
  template <class ResultT, class ClassT, class... ArgsT>
  static auto helper(ResultT (ClassT::*x)(ArgsT...) const) {
    return static_cast<ClassT *>(nullptr);
  }

 public:
  using type = std::remove_pointer_t<decltype(helper(static_cast<FunctionT>(nullptr)))>;
};

template <class FunctionT>
using member_function_class_t = typename member_function_class<FunctionT>::type;

}  // namespace td
