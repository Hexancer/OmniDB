#ifndef ROCKSDB_ES_H
#define ROCKSDB_ES_H

#include <set>

#include "exp_smo.h"

namespace rocksdb {

// ES element definition
//
// Any object that needs to be part of the ES algorithm should extend this
// class
template <class T>
struct ESElement {
  explicit ESElement() {}

  single_exponential_smoothing<double, 1> exp_smoothing_;
};

// ES Implementation
//
// In place ES implementation. There is no copy or allocation involved when
// inserting or removing an element. This makes the data structure slim
template <class T>
class ESHeap {
 public:
  virtual ~ESHeap() {
  }

  // Push element into the ES at the cold end
  inline void Push(T* const t) { the_set_.insert(t); }

  // Unlink the element from the ES
  inline void Unlink(T* const t) { the_set_.erase(t); }

  // Evict an element from the ES
  inline T* Pop() {
    assert(!IsEmpty());
    T* victim = *the_set_.begin();
    Unlink(victim);
    return victim;
  }

  // Move the element from the front of the list to the back of the list
  inline void Touch(T* const t) {
    the_set_.erase(t);
    es_vec<double, 1> observation(1.0);
    t->exp_smoothing_.push_to_pop(observation);
    the_set_.insert(t);
  }

  // Check if the ES is empty
  inline bool IsEmpty() const { return the_set_.empty(); }

 private:
  // Insert an element at the hot end
  inline void PushBack(T* const t) {
    es_vec<double, 1> observation(1.0);
    t->exp_smoothing_.push_to_pop(observation);
    the_set_.insert(t);
  }

  struct CacheNodeComparator {
    bool operator()(T* lhs, T* rhs) const {
      return lhs->exp_smoothing_.ses_curr_smoothed_ob[0] >
             rhs->exp_smoothing_.ses_curr_smoothed_ob[0];
    }
  };

  std::multiset<T*, CacheNodeComparator> the_set_;
};


}  // namespace rocksdb

#endif  // ROCKSDB_ES_H
