//
// Created by pc on 9/11/24.
//

#ifndef ROCKSDB_RR_H
#define ROCKSDB_RR_H

// In place ES implementation. There is no copy or allocation involved when
// inserting or removing an element. This makes the data structure slim
template <class T>
class RRList {
 public:
  virtual ~RRList() {
  }


  Folly_ptr;
  inline void Push(T* const t) { }
  inline void Unlink(T* const t) { }

  inline T* Pop() {
    return ;
  }

  inline void Touch(T* const t) {}

  inline bool IsEmpty() const { return the_set_.empty(); }

};
#endif  // ROCKSDB_RR_H
