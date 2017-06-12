#pragma once

#include "td/utils/common.h"
#include "td/utils/MovableValue.h"
#include "td/utils/Slice-decl.h"

#include <array>

namespace td {

struct DefaultStackAllocatorTag {};
template <class Tag = DefaultStackAllocatorTag>
class StackAllocator {
 public:
  // TODO: alloc memory with mmap and unload unused pages
  class Deleter {
   public:
    void operator()(void *ptr) {
      free_ptr(ptr);
    }
  };
  // memory still can be corrupted, but it is better than explicit free function.
  // TODO: use pointer that can't be even copied

  using PtrImpl = std::unique_ptr<void, Deleter>;
  class Ptr {
   public:
    Ptr() = default;
    Ptr(void *ptr, size_t size) : ptr_(ptr), size_(size) {
    }
    void *get() {
      return ptr_.get();
    }
    MutableSlice as_slice() const {
      return MutableSlice(ptr_.get(), size_.get());
    }
    explicit operator bool() const {
      return static_cast<bool>(ptr_);
    }

   private:
    PtrImpl ptr_;
    MovableValue<size_t> size_;
  };
  static Ptr alloc(size_t size) {
    return Ptr(impl().alloc(size), size);
  }

 private:
  static void free_ptr(void *ptr) {
    impl().free_ptr(ptr);
  }
  struct Impl {
    static const size_t MEM_SIZE = 1024 * 1024;
    std::array<char, MEM_SIZE> mem;

    size_t pos{0};
    void *alloc(size_t size) {
      if (size == 0) {
        size = 1;
      }
      void *res = mem.data() + pos;
      size = (size + 7) & -8;
      pos += size;
      ASSERT_CHECK(pos < MEM_SIZE);
      return res;
    }
    void free_ptr(void *ptr) {
      size_t new_pos = static_cast<char *>(ptr) - mem.data();
      ASSERT_CHECK(new_pos < MEM_SIZE);
      ASSERT_CHECK(new_pos < pos);
      pos = new_pos;
    }
  };
  static Impl &impl() {
    init_thread_local<Impl>(impl_);
    return *impl_;
  }
  static TD_THREAD_LOCAL Impl *impl_;
};

template <class Tag>
TD_THREAD_LOCAL typename StackAllocator<Tag>::Impl *StackAllocator<Tag>::impl_;  // static zero initialized

}  // namespace td
