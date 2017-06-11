#pragma once

#include "td/utils/common.h"
#include "td/utils/misc.h"

#include <tuple>
#include <type_traits>

//
// Essentially we have:
// (ActorT::func, arg1, arg2, ..., argn)
// We want to call:
// actor->func(arg1, arg2, ..., argn)
// And in some cases we would like to delay this call.
//
// First attempt would be
// [a1=arg1, a2=arg2, ..., an=argn](ActorT *actor) {
//   actor->func(a1, a2, ..., an)
// }
//
// But there are some difficulties with elimitation on unnecessary copies.
// We want to use move constructor when it is possible
//
// We may pass
// Tmp. Temporary / rvalue reference
// Var. Variable / reference
// CnstRef. const reference
//
//
// Function may expect
// Val. Value
// CnstRef. const reference
// Ref. rvalue reverence / reference
//
// TODO:
//    Immediate call / Delayed call
// Tmp->Val       move / move->move
// Tmp->CnstRef      + / move->+
// Tmp->Ref          + / move->+
// Var->Val       copy / copy->move
// Var->CnstRef      + / copy->
// Var->Ref          + / copy->+   // khm. It will complile, but won't work
//
// So I will use common idiom: forward references
// If delay is needed, just std::forward data to temporary storage, and sdt::move them when call
// is executed.
//
//
// create_immediate_closure(&Actor::func, arg1, arg2, ..., argn).run(actor)
// to_delayed_closure(std::move(immediate)).run(actor)

namespace td {
template <class ActorT, class FunctionT, class... ArgsT>
class DelayedClosure;

template <class ActorT, class FunctionT, class... ArgsT>
class ImmediateClosure {
 public:
  using Delayed = DelayedClosure<ActorT, FunctionT, ArgsT...>;
  friend Delayed;
  using ActorType = ActorT;

  void run(ActorT *actor) {
    mem_call_tuple(actor, func, std::move(args));
  }

  // no &&. just save references as references.
  explicit ImmediateClosure(FunctionT func, ArgsT... args) : func(func), args(std::forward<ArgsT>(args)...) {
  }

 private:
  FunctionT func;
  std::tuple<ArgsT...> args;
};

template <class ActorT, class ResultT, class... DestArgsT, class... SrcArgsT>
ImmediateClosure<ActorT, ResultT (ActorT::*)(DestArgsT...), SrcArgsT &&...> create_immediate_closure(
    ResultT (ActorT::*func)(DestArgsT...), SrcArgsT &&... args) {
  return ImmediateClosure<ActorT, ResultT (ActorT::*)(DestArgsT...), SrcArgsT &&...>(func,
                                                                                     std::forward<SrcArgsT>(args)...);
}

template <class ActorT, class FunctionT, class... ArgsT>
class DelayedClosure {
 public:
  using ActorType = ActorT;
  using Delayed = DelayedClosure<ActorT, FunctionT, ArgsT...>;

  void run(ActorT *actor) {
    mem_call_tuple(actor, func, std::move(args));
  }

  DelayedClosure clone() const {
    return do_clone(*this);
  }

  explicit DelayedClosure(ImmediateClosure<ActorT, FunctionT, ArgsT...> &&other)
      : func(std::move(other.func)), args(std::move(other.args)) {
  }

  explicit DelayedClosure(FunctionT func, ArgsT... args) : func(func), args(std::forward<ArgsT>(args)...) {
  }

  template <class F>
  void for_each(const F &f) {
    tuple_for_each(args, f);
  }

 private:
  using ArgsStorageT = std::tuple<typename std::decay<ArgsT>::type...>;

  FunctionT func;
  ArgsStorageT args;

  template <class FromActorT, class FromFunctionT, class... FromArgsT>
  explicit DelayedClosure(const DelayedClosure<FromActorT, FromFunctionT, FromArgsT...> &other,
                          std::enable_if_t<LogicAnd<std::is_copy_constructible<FromArgsT>::value...>::value, int> = 0)
      : func(other.func), args(other.args) {
  }

  template <class FromActorT, class FromFunctionT, class... FromArgsT>
  explicit DelayedClosure(
      const DelayedClosure<FromActorT, FromFunctionT, FromArgsT...> &other,
      std::enable_if_t<!LogicAnd<std::is_copy_constructible<FromArgsT>::value...>::value, int> = 0) {
    UNREACHABLE("deleted constructor");
  }

  template <class FromActorT, class FromFunctionT, class... FromArgsT>
  std::enable_if_t<!LogicAnd<std::is_copy_constructible<FromArgsT>::value...>::value,
                   DelayedClosure<FromActorT, FromFunctionT, FromArgsT...>>
  do_clone(const DelayedClosure<FromActorT, FromFunctionT, FromArgsT...> &value) const {
    UNREACHABLE("Trying to clone DelayedClosure that contains noncopyable elements");
  }

  template <class FromActorT, class FromFunctionT, class... FromArgsT>
  std::enable_if_t<LogicAnd<std::is_copy_constructible<FromArgsT>::value...>::value,
                   DelayedClosure<FromActorT, FromFunctionT, FromArgsT...>>
  do_clone(const DelayedClosure<FromActorT, FromFunctionT, FromArgsT...> &value) const {
    return DelayedClosure<FromActorT, FromFunctionT, FromArgsT...>(value);
  }
};

template <class ImmediateClosureT>
typename ImmediateClosureT::Delayed to_delayed_closure(ImmediateClosureT &&other) {
  return typename ImmediateClosureT::Delayed(std::forward<ImmediateClosureT>(other));
}

template <class ActorT, class ResultT, class... DestArgsT, class... SrcArgsT>
auto create_delayed_closure(ResultT (ActorT::*func)(DestArgsT...), SrcArgsT &&... args) {
  return DelayedClosure<ActorT, ResultT (ActorT::*)(DestArgsT...), SrcArgsT &&...>(func,
                                                                                   std::forward<SrcArgsT>(args)...);
}

}  // namespace td
