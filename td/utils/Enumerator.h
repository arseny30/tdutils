#pragma once

#include "td/utils/common.h"
#include "td/utils/logging.h"
#include "td/utils/misc.h"

#include <map>
#include <tuple>

namespace td {

template <class ValueT>
class Enumerator {
 public:
  using Key = int32;

  Key add(ValueT v) {
    int32 next_id = narrow_cast<int32>(arr_.size() + 1);
    bool was_inserted;
    decltype(map_.begin()) it;
    std::tie(it, was_inserted) = map_.emplace(std::move(v), next_id);
    if (was_inserted) {
      arr_.push_back(&it->first);
    }
    return it->second;
  }

  const ValueT &get(Key key) const {
    auto pos = static_cast<size_t>(key - 1);
    CHECK(pos < arr_.size());
    return *arr_[pos];
  }

 private:
  std::map<ValueT, int32> map_;
  std::vector<const ValueT *> arr_;
};

}  // namespace td
