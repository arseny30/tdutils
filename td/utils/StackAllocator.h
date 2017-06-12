#pragma once

#include "td/utils/common.h"
#include "td/utils/MovableValue.h"
#include "td/utils/Slice-decl.h"

#include <array>

namespace td {

struct DefaultStackAllocatorTag {};
template <class Tag = DefaultStackAllocatorTag>
class StackAllocator {
  class Deleter {
   public:
    void operator()(char *ptr) {
      free_ptr(ptr);
    }
  };

  // TODO: alloc memory with mmap and unload unused pages
  // memory still can be corrupted, but it is better than explicit free function
  // TODO: use pointer that can't be even copied
  using PtrImpl = std::unique_ptr<char, Deleter>;
  class Ptr {
   public:
    Ptr(char *ptr, size_t size) : ptr_(ptr), size_(size) {
    }

    MutableSlice as_slice() const {
      return MutableSlice(ptr_.get(), size_.get());
    }

   private:
    PtrImpl ptr_;
    MovableValue<size_t> size_;
  };

  static void free_ptr(char *ptr) {
    impl().free_ptr(ptr);
  }

  struct Impl {
    static const size_t MEM_SIZE = 1024 * 1024;
    std::array<char, MEM_SIZE> mem;

    size_t pos{0};
    char *alloc(size_t size) {
      if (size == 0) {
        size = 1;
      }
      char *res = mem.data() + pos;
      size = (size + 7) & -8;
      pos += size;
      ASSERT_CHECK(pos < MEM_SIZE);
      return res;
    }
    void free_ptr(char *ptr) {
      size_t new_pos = ptr - mem.data();
      ASSERT_CHECK(new_pos < MEM_SIZE);
      ASSERT_CHECK(new_pos < pos);
      pos = new_pos;
    }
  };

  static Impl &impl() {
    static TD_THREAD_LOCAL Impl *impl_;  // static zero initialized
    init_thread_local<Impl>(impl_);
    return *impl_;
  }

 public:
  static Ptr alloc(size_t size) {
    return Ptr(impl().alloc(size), size);
  }
};

}  // namespace td
