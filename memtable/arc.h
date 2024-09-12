//
// Created by geng on 9/3/24.
//

#ifndef ROCKSDB_ARC_H
#define ROCKSDB_ARC_H

#include <algorithm>
#include <atomic>
#include <cassert>
#include <iostream>
#include <unordered_map>

namespace rocksdb {
// ARC element definition
//
// Any object that needs to be part of the ARC algorithm should use this
template <class T>
struct ARCElement {
  explicit ARCElement() : next_(nullptr), prev_(nullptr), refs_(0) {}

  virtual ~ARCElement() { assert(!refs_); }

  T* next_;
  T* prev_;
  std::atomic<size_t> refs_;
};

// ARC list implementation
//
// In-place ARC implementation. There is no copy or allocation involved when
// inserting or removing an element. This makes the data structure slim.
template <class T>
class ARCList {
 public:
  ARCList() : head_(nullptr), tail_(nullptr), size_(0) {}

  virtual ~ARCList() {
    assert(!head_);
    assert(!tail_);
  }

  T* Find(const T* element) const {
    T* current = head_;
    while (current) {
      if (current == element) {
        return current;
      }
      current = current->next_;
    }
    return nullptr;
  }
  // Push element into the ARC at the hot end (MRU end)
  inline void PushFront(T* const t) {
    assert(t);
    assert(!t->next_);
    assert(!t->prev_);

    if (head_) {
      head_->prev_ = t;
    }
    t->next_ = head_;
    head_ = t;
    if (!tail_) {
      tail_ = t;
    }
    ++size_;
  }

  // Unlink the element from the ARC list
  inline void Unlink(T* const t) { UnlinkImpl(t); }

  // Evict an element from the ARC list
  inline T* PopBack() {
    assert(tail_ && head_);
    T* t = tail_;
    UnlinkImpl(t);
    return t;
  }

  // Check if the ARC list is empty
  inline bool IsEmpty() const { return !head_ && !tail_; }

  inline size_t Size() const { return size_; }

  T* head_;

 private:
  // Unlink an element from the ARC list
  void UnlinkImpl(T* const t) {
    assert(t);

    if (t->prev_) {
      t->prev_->next_ = t->next_;
    }
    if (t->next_) {
      t->next_->prev_ = t->prev_;
    }

    if (tail_ == t) {
      tail_ = tail_->prev_;
    }
    if (head_ == t) {
      head_ = head_->next_;
    }

    t->next_ = t->prev_ = nullptr;
    --size_;
  }

  T* tail_;
  size_t size_;
};

// ARC cache implementation
template <class T>
class ARCCache {
 public:
  explicit ARCCache(size_t cache_size) : cache_size_(cache_size), p_(0) {}

  ~ARCCache() {
    ClearList(t1_);
    ClearList(t2_);
    ClearList(b1_);
    ClearList(b2_);
  }
  void PrintCache() const {
    std::cout << "T1 (Most Recently Used):" << std::endl;
    PrintList(t1_);

    std::cout << "T2 (Most Frequently Used):" << std::endl;
    PrintList(t2_);

    std::cout << "B1 (Ghost of T1):" << std::endl;
    PrintList(b1_);

    std::cout << "B2 (Ghost of T2):" << std::endl;
    PrintList(b2_);

    std::cout << "p (target size for T1): " << p_ << std::endl;
    std::cout << "Cache size: " << cache_size_ << std::endl;
  }
  void PrintList(const ARCList<T>& list) const {
    T* current = list.head_;
    while (current) {
      std::cout << "Key: " << current->Key()
                << std::endl;  // 假设 T 有一个 key 成员
      current = current->next_;
    }
  }
  void Access(T* const element) {
    if (FindInList(t1_, element)) {
      MoveToFront(t1_, element);
      MoveToFront(t2_, element);
      return;
    }

    if (FindInList(t2_, element)) {
      MoveToFront(t2_, element);
      return;
    }

    if (FindInList(b1_, element)) {
      p_ = std::min(
          static_cast<double>(cache_size_),  // 将 cache_size_ 转换为 double
          p_ + std::max(1.0, b2_.Size() / static_cast<double>(b1_.Size())));

      Replace(element);
      MoveToFront(t2_, element);
      return;
    }

    if (FindInList(b2_, element)) {
      p_ = std::max(
          0.0,
          p_ - std::max(1.0, b1_.Size() / static_cast<double>(b2_.Size())));
      Replace(element);
      MoveToFront(t2_, element);
      return;
    }

    if ((t1_.Size() + b1_.Size()) == cache_size_) {
      if (t1_.Size() < cache_size_) {
        b1_.PopBack();
      } else {
        t1_.PopBack();
      }
    } else if ((t1_.Size() + b1_.Size()) < cache_size_ &&
               (t1_.Size() + t2_.Size() + b1_.Size() + b2_.Size()) >=
                   cache_size_) {
      if ((t1_.Size() + t2_.Size() + b1_.Size() + b2_.Size()) ==
          2 * cache_size_) {
        b2_.PopBack();
      }
      Replace(element);
    }

    MoveToFront(t1_, element);
//    std::cout << "After Access:" << std::endl;
//    PrintCache();
  }

  T* Evict() {
    T* evicted = nullptr;
    if (t1_.Size() > 0 &&
        (t1_.Size() > p_ || (b2_.Find(evicted) && t1_.Size() == p_))) {
      // 从 T1 列表中驱逐元素
      evicted = t1_.PopBack();
      b1_.PushFront(evicted);
    } else if (t2_.Size() > 0) {
      // 从 T2 列表中驱逐元素
      evicted = t2_.PopBack();
      b2_.PushFront(evicted);
    }
    return evicted;  // 返回被驱逐的元素
  }

 private:
  size_t cache_size_;
  double p_;

  ARCList<T> t1_, t2_, b1_, b2_;

  void Replace(T* const element) {
    if (t1_.Size() > 0 &&
        (t1_.Size() > p_ || (FindInList(b2_, element) && t1_.Size() == p_))) {
      b1_.PushFront(t1_.PopBack());
    } else {
      b2_.PushFront(t2_.PopBack());
    }
  }

  void MoveToFront(ARCList<T>& list, T* const element) {
    list.Unlink(element);
    list.PushFront(element);
  }
  bool FindInList(ARCList<T>& list, T* const element) {
    T* current = list.head_;
    while (current) {
      if (current->Key() == element->Key()) {
        return true;
      }
      current = current->next_;
    }
    return false;
  }

  void ClearList(ARCList<T>& list) {
    while (!list.IsEmpty()) {
      delete list.PopBack();
    }
  }
};
}  // namespace rocksdb
#endif  // ROCKSDB_ARC_H
