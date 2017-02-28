#pragma once

#include "td/utils/Slice.h"

namespace td {
class Storer {
 public:
  Storer() = default;
  Storer(const Storer &) = delete;
  Storer &operator=(const Storer &) = delete;
  Storer(Storer &&) = default;
  Storer &operator=(Storer &&) = default;
  virtual ~Storer() = default;
  virtual size_t size() const = 0;
  virtual size_t store(uint8 *ptr) const = 0;
};
}