#pragma once

namespace td {

template <class FunctionT>
struct member_function_class;

template <class ReturnType, class Type, class... Args>
struct member_function_class<ReturnType (Type::*)(Args...)> {
  using type = Type;
  constexpr static size_t arguments_count() {
    return sizeof...(Args);
  }
};

template <class FunctionT>
using member_function_class_t = typename member_function_class<FunctionT>::type;

template <class FunctionT>
constexpr size_t member_function_arguments_count() {
  return member_function_class<FunctionT>::arguments_count();
}

}  // namespace td
