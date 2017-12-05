#pragma once

namespace td {

template <class FunctionT>
struct member_function_class;

template <class ReturnType, class Type>
struct member_function_class<ReturnType Type::*> {
  using type = Type;
};

template <class FunctionT>
using member_function_class_t = typename member_function_class<FunctionT>::type;

}  // namespace td
