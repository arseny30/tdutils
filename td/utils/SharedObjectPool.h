#include "td/utils/common.h"

#include "td/utils/MpscLinkQueue.h"

namespace td {
namespace detail {
class AtomicRefCnt {
 public:
  AtomicRefCnt(uint64 cnt) : cnt_(cnt) {
  }
  void inc() {
    cnt_.fetch_add(1, std::memory_order_relaxed);
  }
  bool dec() {
    return cnt_.fetch_add(-1, std::memory_order_acq_rel) == 1;
  }

 private:
  std::atomic<uint64> cnt_;
};

template <class DataT, class DeleterT>
class SharedPtrRaw : public DeleterT, private MpscLinkQueueImpl::Node {
 public:
  template <class... ArgsT>
  SharedPtrRaw(DeleterT deleter, ArgsT &&... args)
      : DeleterT(std::move(deleter)), ref_cnt_{0}, data_(std::forward<ArgsT>(args)...) {
  }
  void inc() {
    ref_cnt_.inc();
  }
  bool dec() {
    return ref_cnt_.dec();
  }
  DataT &data() {
    return data_;
  }
  static SharedPtrRaw *from_mpsc_link_queue_node(MpscLinkQueueImpl::Node *node) {
    return static_cast<SharedPtrRaw<DataT, DeleterT> *>(node);
  }
  MpscLinkQueueImpl::Node *to_mpsc_link_queue_node() {
    return static_cast<MpscLinkQueueImpl::Node *>(this);
  }

 private:
  AtomicRefCnt ref_cnt_;
  DataT data_;
};

template <class T, class DeleterT = std::default_delete<T>>
class SharedPtr {
 public:
  using Raw = detail::SharedPtrRaw<T, DeleterT>;
  SharedPtr() = default;
  ~SharedPtr() {
    if (!raw_) {
      return;
    }
    reset();
  }
  explicit SharedPtr(Raw *raw) : raw_(raw) {
    raw_->inc();
  }
  SharedPtr(const SharedPtr &other) : SharedPtr(other.raw_) {
  }
  SharedPtr &operator=(const SharedPtr &other) {
    other.raw_->inc();
    reset(other.raw_);
    return *this;
  }
  SharedPtr(SharedPtr &&other) : raw_(other.raw_) {
    other.raw_ = nullptr;
  }
  SharedPtr &operator=(SharedPtr &&other) {
    reset(other.raw_);
    other.raw_ = nullptr;
    return *this;
  }
  bool empty() const {
    return raw_ == nullptr;
  }
  operator bool() const {
    return !empty();
  }
  T &operator*() const {
    return raw_->data();
  }
  T *operator->() const {
    return &raw_->data();
  }

  Raw *release() {
    auto res = raw_;
    raw_ = nullptr;
    return res;
  }

  void reset(Raw *new_raw = nullptr) {
    if (raw_ && raw_->dec()) {
      auto deleter = std::move(static_cast<DeleterT &>(*raw_));
      deleter(raw_);
    }
    raw_ = new_raw;
  }

  template <class... ArgsT>
  static SharedPtr<T, DeleterT> create(ArgsT &&... args) {
    return SharedPtr<T, DeleterT>(std::make_unique<Raw>(DeleterT(), std::forward<ArgsT>(args)...).release());
  }
  template <class D, class... ArgsT>
  static SharedPtr<T, DeleterT> create_with_deleter(D &&d, ArgsT &&... args) {
    return SharedPtr<T, DeleterT>(std::make_unique<Raw>(std::forward<D>(d), std::forward<ArgsT>(args)...).release());
  }

 private:
  Raw *raw_{nullptr};
};

}  // namespace detail

template <class DataT>
class SharedObjectPool {
  class Deleter;

 public:
  using Ptr = detail::SharedPtr<DataT, Deleter>;

  Ptr alloc() {
    return Ptr(alloc_raw());
  }

 private:
  using Raw = typename Ptr::Raw;
  Raw *alloc_raw() {
    free_queue_.pop_all(free_queue_reader_);
    auto *raw = free_queue_reader_.read().get();
    if (raw) {
      return raw;
    }
    allocated_.push_back(std::make_unique<Raw>(deleter()));
    return allocated_.back().get();
  }

  void free_raw(Raw *raw) {
    raw->data() = DataT();
    free_queue_.push(Node{raw});
  }

  class Node {
   public:
    Node() = default;
    explicit Node(Raw *raw) : raw_(raw) {
    }

    MpscLinkQueueImpl::Node *to_mpsc_link_queue_node() {
      return raw_->to_mpsc_link_queue_node();
    }
    static Node from_mpsc_link_queue_node(MpscLinkQueueImpl::Node *node) {
      return Node{Raw::from_mpsc_link_queue_node(node)};
    }
    Raw *get() const {
      return raw_;
    }
    operator bool() const {
      return bool(raw_);
    }

   private:
    Raw *raw_{nullptr};
  };

  class Deleter {
   public:
    explicit Deleter(SharedObjectPool<DataT> *pool) : pool_(pool) {
    }
    void operator()(Raw *raw) {
      pool_->free_raw(raw);
    };

   private:
    SharedObjectPool<DataT> *pool_;
  };
  friend class Deleter;

  auto deleter() {
    return Deleter(this);
  }

  std::vector<std::unique_ptr<Raw>> allocated_;
  MpscLinkQueue<Node> free_queue_;
  typename MpscLinkQueue<Node>::Reader free_queue_reader_;
};

}  // namespace td
