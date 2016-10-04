#pragma once
#include "td/utils/common.h"
#include "td/utils/Slice-decl.h"

namespace td {

struct DefaultStackAllocatorTag {};
template <class Tag = DefaultStackAllocatorTag>
class StackAllocator {
 public:
  // TODO: alloc memory with mmap and unload unused pages
  class Deleter {
   public:
    void operator()(void *ptr) {
      impl_.free(ptr);
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
    MutableSlice as_slice() {
      return MutableSlice(ptr_.get(), size_.get());
    }
    operator bool() {
      return bool(ptr_);
    }

   private:
    PtrImpl ptr_;
    MovableValue<size_t> size_;
  };
  static Ptr alloc(size_t size) {
    return Ptr(impl_.alloc(size), size);
  }

 private:
  static void free(void *ptr) {
    impl_.free(ptr);
  }
  struct Impl {
    static const size_t MEM_SIZE = 1024 * 1024;
#if TD_MAC
    char *mem;
#else
    char mem[MEM_SIZE];
#endif

    size_t pos;  // Impl is static, so pos == 0.
    void *alloc(size_t size) {
#if TD_MAC
      if (!mem) {
        mem = new char[MEM_SIZE];
      }
#endif
      if (size == 0) {
        size = 1;
      }
      void *res = mem + pos;
      size = (size + 7) & -8;
      pos += size;
      ASSERT_CHECK(pos < MEM_SIZE);
      return res;
    }
    void free(void *ptr) {
      size_t new_pos = static_cast<char *>(ptr) - mem;
      ASSERT_CHECK(new_pos < MEM_SIZE);
      ASSERT_CHECK(new_pos < pos);
      pos = new_pos;
    }
  };
  static TD_THREAD_LOCAL Impl impl_;
};

template <class Tag>
TD_THREAD_LOCAL typename StackAllocator<Tag>::Impl StackAllocator<Tag>::impl_;

}  // end of namespace td
