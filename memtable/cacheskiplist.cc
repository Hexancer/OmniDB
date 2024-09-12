#include "memtable/cacheskiplist.h"

#include <iostream>
#include <map>

#include "rocksdb/comparator.h"
#include "rocksdb/debug.h"
#include "rocksdb/macro.h"
#include "rocksdb/perf_data_client.h"

namespace rocksdb {

std::ostream& operator<<(std::ostream& os, const CacheSkipListNode& node) {
  if (node.IsSentinel()) {
    os << "|";
  } else {
    os << '"' << node.Key() << '"';
  }
  return os;
}

CacheSkipListStats::CacheSkipListStats() {
  auto& client = PerfDataClient::GetPerfDataClient();

#define REGISTER_METRIC(m)                                         \
  do {                                                             \
    std::function<double()> func = [this]() { return (double)m; }; \
    client.RegisterMetric("/oc/skiplist/" #m, func);               \
  } while (0)

#define REGISTER_LEVEL_METRIC(m, l) REGISTER_METRIC(m[l])

#define REGISTER_LEVEL_LENGTH(l) REGISTER_LEVEL_METRIC(levelLength_, l)
  FOREACH0(REGISTER_LEVEL_LENGTH, CACHESKIPLIST_MAXLEVEL);
}


CacheSkipList::CacheSkipList(int maxLevel, const Comparator* cmp,
                             size_t maxsize)
    : maxLevel_(maxLevel), currentLevel_(0), cmp_(cmp), maxSize_(maxsize) {
  head_ = new CacheSkipListNode(Slice(), Slice(), maxLevel);
  tail_ = new CacheSkipListNode(Slice(TAIL), Slice(), maxLevel);
  for (size_t i = 0; i < head_->next_.size() + 1; i++) {
    head_->Next(i) = tail_;
  }
  InsertSentinel(head_);
  PrintAllNodes();

  srand(42);
  dbglprintf("cache maxsize : %lu \n", maxSize_);
}

CacheSkipList::~CacheSkipList() {
  DestroyAllNodes();
  stats_.Dump();
}

void CacheSkipList::DestroyAllNodes() {
  NodePtrType ptr = head_;
  while (ptr != nullptr) {
    NodePtrType next = ptr->Next(0);
    stats_.OnUnlinkNode(ptr);
    if (ptr->IsSentinel()) {
      DeallocateSentinel(ptr);
    } else {
      DeallocateNode(ptr);
    }
    ptr = next;
  }
}

void CacheSkipList::LinkNode(CacheSkipList::NodePtrType newNode, int level,
                             const std::vector<NodePtrType>& preds) {
  std::vector<NodePtrType> succs(level);
  for (int i = 0; i < level; i++) {
    succs[i] = preds[i]->Next(i);
  }

  LinkNode(newNode, level, preds, succs);
}

void CacheSkipList::LinkNode(NodePtrType newNode,
                             NodePtrType pred,
                             NodePtrType succ) {
  // Specialized version for level 0 nodes (mostly sentinels)
  newNode->Next(0) = succ;
  pred->Next(0) = newNode;

  newNode->fully_linked_ = true;
  stats_.OnLinkNode(newNode);
}

void CacheSkipList::LinkNode(CacheSkipList::NodePtrType newNode, int level,
                             const std::vector<NodePtrType>& preds,
                             const std::vector<NodePtrType>& succs) {
  // Update the predecessor and successors
  for (int i = 0; i < level; i++) {
    assert(succs[i] != nullptr);
    newNode->Next(i) = succs[i];
  }
  for (int i = 0; i < level; i++) {
    preds[i]->Next(i) = newNode;
  }

  newNode->fully_linked_ = true;
  stats_.OnLinkNode(newNode);
}

void CacheSkipList::UnlinkNode(CacheSkipList::NodePtrType x,
                               std::vector<NodePtrType> preds) {
  assert(x != head_);
  assert(x != tail_);
  for (int i = 0; i <= currentLevel_; i++) {
    if (preds[i]->Next(i) != x) {
      break;
    }
    preds[i]->Next(i) = x->Next(i);
  }
  MaybeDecreaseHeight();
  stats_.OnUnlinkNode(x);
}

void CacheSkipList::InsertSentinel(CacheSkipList::NodePtrType node) {
  std::vector<NodePtrType> preds{node};
  auto sentinel = AllocateSentinel(1);
  LinkNode(sentinel, 1, preds);
}

void CacheSkipList::UnlinkSentinel(CacheSkipList::NodePtrType prev, CacheSkipList::NodePtrType node) {
  prev->Next(0) = node->Next(0);
  stats_.OnUnlinkNode(node);
}

void CacheSkipList::DeleteSentinel(CacheSkipList::NodePtrType  prev, CacheSkipList::NodePtrType node) {
  assert(node->IsSentinel());
  UnlinkSentinel(prev, node);
  DeallocateSentinel(node);
}

void __attribute__((noinline)) CacheSkipList::PrintAllNodes() const {
  dbglprintf("\n");
  for (int i = 0; i < maxLevel_; ++i) {
    dbglprintf("%d\n", i);
    auto x = head_;
    while (x != nullptr) {
      if (x->IsSentinel()) {
        if (x->deleted_) {
          dbgprintf("%s(%u)(D) -> ", SENTINEL_STR, x->se_.ea_stats_.length_);
        } else {
          dbgprintf("%s(%u) -> ", SENTINEL_STR, x->se_.ea_stats_.length_);
        }
      } else {
        if (x->deleted_) {
          dbgprintf("%s(D) -> ", x->Key().c_str());
        } else {
          dbgprintf("%s -> ", x->Key().c_str());
        }
      }
      if (i >= (int)x->Height()) break;
      x = x->Next(i);
    }
    dbgprintf("\n");
  }
}

std::vector<std::string> CacheSkipList::DumpAllNodes() const {
  std::vector<std::string> xs;
  auto x = head_;
  while (x != nullptr) {
    if (x->IsSentinel()) {
      xs.emplace_back(SENTINEL_STR);
    } else {
      xs.push_back(x->Key());
    }
    x = x->Next(0);
  }
  return xs;
}

std::vector<std::string> CacheSkipList::DumpAllNodesNoSentinel() const {
  std::vector<std::string> xs;
  auto x = head_;
  while (x != nullptr) {
    if (!x->IsSentinel()) xs.push_back(x->Key());
    x = x->Next(0);
  }
  return xs;
}

// Find the largest (entry or sentinel) < key
CacheSkipList::NodePtrType CacheSkipList::Find(const Slice& key) {
  std::vector<NodePtrType> preds(maxLevel_, nullptr);
  return Find(key, preds);
}

CacheSkipList::NodePtrType CacheSkipList::Find(
    const Slice& key, std::vector<NodePtrType>& preds) {
  NodePtrType prev;
  std::vector<NodePtrType> succs(maxLevel_, nullptr);
  int found = -1;
  return Find(key, prev, preds, succs, found);
}

void CacheSkipList::Find(CacheSkipList::NodePtrType ptr,
                         std::vector<NodePtrType>& preds) {
  Find(ptr->Key(), preds);
}

CacheSkipList::NodePtrType CacheSkipList::Find(const Slice& key,
                                               NodePtrType& prev,
                                               std::vector<NodePtrType>& preds,
                                               std::vector<NodePtrType>& succs,
                                               int& found) {
  found = -1;

  prev = head_;
  NodePtrType x = head_, next = nullptr;

  for (int i = currentLevel_; i >= 0; i--) {
    next = x->Next(i);
    while (cmpWrapper(next, key) < 0) {
      assert(x->deleted_ == false);
      prev = x;
      x = next;
      next = x->Next(i);
    }

    if (found == -1) {
      if (cmpWrapper(next, key) == 0) {
        found = i;
      }
    }

    preds[i] = x;
    succs[i] = next;
  }

  return x;
}

std::vector<CacheSkipList::NodePtrType> CacheSkipList::FindNext(
    CacheSkipList::NodePtrType ptr, int maxFillLevel) {
  // next[i] will be in [ptr, )
  // especially, next[0] will be ptr

  std::vector<NodePtrType> next(maxFillLevel, nullptr);

  int idx = 0;
  while (idx < maxFillLevel && ptr != nullptr) {
    // try to fill next with current node
    while (idx < ptr->Height() && idx < maxFillLevel) {
      next[idx] = ptr;
      idx++;
    }
    // go until we find a higher node
    while (ptr != nullptr && ptr->Height() == idx) {
      ptr = ptr->Next(idx - 1);
    }
  }

  return next;
}

CacheSkipList::NodePtrType CacheSkipList::Seek(const Slice& key) {
  std::vector<NodePtrType> preds(maxLevel_, nullptr);
  return Seek(key, preds);
}

CacheSkipList::NodePtrType CacheSkipList::Seek(
    const Slice& key, std::vector<NodePtrType>& preds) {
  NodePtrType prev;
  std::vector<NodePtrType> succs(maxLevel_, nullptr);
  int found = -1;

  auto x = Find(key, prev, preds, succs, found);
  auto next = succs[0];

  if (!x->IsSentinel()) {
    // ..., (x < key), (b >= key)
    //    Find ensures that b is not sentinel
    //    goto b
    x = next;
  } else {
    if (cmpWrapper(next, key) == 0) {
      // ..., |, key
      //    goto key
      x = next;
    } else {
      // ..., (a < key), |, (b > key)
      //    key not in closed ranges
      //    do nothing
    }
  }

  return x;
}

bool CacheSkipList::ShouldEvict() const {
  return stats_.valueSize_.Value() > maxSize_;
}

void CacheSkipList::Evict() {
  static NodePtrType ptr = head_;
  if (ptr == tail_) {
    ptr = this->head_->Next(0)->Next(0);
  }

  ptr = DeleteRange(ptr, 4096);

  // ..., a, (| = ptr), ...
  //  if there remains a sentinel at ptr, after eviction,
  //  remove it together to prevent consecutive sentinels
  if (ptr != tail_ && ptr->IsSentinel()) {
    auto next = ptr->Next(0);
    DeleteSentinel(ptr, next);
    ptr = next;
  }
}

std::unique_ptr<CacheSkipListIterator> CacheSkipList::Insert(
    const Slice& key, const Slice& value) {
  stats_.OnInsert();
  //    if (ShouldEvict()) {
  //      Evict();
  //      stats_.OnEvict();
  //    }

  std::unique_ptr<CacheSkipListIterator> iter{nullptr};

  // Callback on found
  auto pfFound = [this, &value, &iter](NodePtrType prev,
                                       std::vector<NodePtrType>& preds,
                                       std::vector<NodePtrType>& succs) {
    auto node_found = succs[0];
    M_UpdateValue(node_found, value);
    iter = NewIterator(node_found);
    return true;
  };

  // Callback on not found
  auto pfElse = [this, &key, &value, &iter](NodePtrType prev,
                                            std::vector<NodePtrType>& preds,
                                            std::vector<NodePtrType>& succs) {
    auto x = preds[0];
    assert(x != nullptr);
    assert(x != tail_);
    assert(cmpWrapper(x, key) != 0);
    // x is (end)
    // or ..., (a < key), (b > key)

    int level = preds.size();
    auto pfInsert = [this, key, value, &preds, &succs, &level, &iter]() {
      auto newNode = AllocateNode(key, value, level);
      auto sentinel = AllocateSentinel(1);

      // link sentinel first
      LinkNode(sentinel, newNode, succs[0]);

      // then link node
      auto next = succs[0];
      succs[0] = sentinel;
      LinkNode(newNode, level, preds, succs);
      succs[0] = next;
      iter = NewIterator(newNode);
    };
    bool success = M_LockedExec(prev, preds, succs, level, pfInsert);
    return success;
  };

  bool found = M_RetryFind(key, pfFound, pfElse);
  assert(found);
  return iter;

}

std::unique_ptr<CacheSkipListIterator> CacheSkipList::Append(
    const Slice& key, const Slice& value) {
  stats_.OnAppend();

  std::unique_ptr<CacheSkipListIterator> iter{nullptr};

  auto pfFound = [this, &key, &value, &iter](NodePtrType prev,
                                             std::vector<NodePtrType>& preds,
                                             std::vector<NodePtrType>& succs) {
    auto x = preds[0];
    auto next = succs[0];

    if (x->IsSentinel()) {
      // ..., |, key,
      // key is exactly the [lo] of next range
      // remove the sentinel in this case

      if (cmpWrapper(next, key) == 0) {
        // Key is exactly the [lo] of next range, remove the sentinel
        // TODO: this may fail since already removed
        M_DeleteSentinel(prev, x);
        iter = NewIterator(next);
        return true;
      }
    } else {
      assert(cmpWrapper(next, key) == 0);

      // Update key itself
      M_UpdateValue(next, value);
      iter = NewIterator(next);
      return true;
    }
    return false;
  };

  auto pfElse = [this, &key, &value, &iter](NodePtrType prev,
                                            std::vector<NodePtrType>& preds,
                                            std::vector<NodePtrType>& succs) {
    auto x = preds[0];
    auto next = succs[0];
    if (x->IsSentinel()) {
      assert(x->IsSentinel());
      assert(prev != nullptr && !prev->IsSentinel());

      int level = preds.size();

      auto func = [this, &key, &value, level, &preds, &succs, prev, x, &next,
          &iter]() {
        preds[0] = prev;
        succs[0] = x;

        NodePtrType newNode = AllocateNode(key, value, level);
        LinkNode(newNode, level, preds, succs);

        // restore preds and succs,
        // they will be used in unlocking
        preds[0] = x;
        succs[0] = next;
        iter = NewIterator(newNode);
      };

      bool success = M_LockedExec(prev, preds, succs, level, func);
      return success;
    } else {
      return false;
    }
  };

  bool found = M_RetryFind(key, pfFound, pfElse);
  assert(found);
  return iter;

}

int CacheSkipList::RandomUpdateLevel() {
  int level = RandomLevel();
  if (level > currentLevel_) {
    for (int i = currentLevel_ + 1; i <= level; ++i) {
      head_->Next(i) = tail_;
    }
    currentLevel_ = level;
  }
  return level;
}

void CacheSkipList::ResizeToLevel(std::vector<NodePtrType>& preds, int level) {
  preds.resize(level);
  for (int i = level - 1; i >= 0 && preds[i] == nullptr; --i) {
    preds[i] = head_;
  }
}

void CacheSkipList::MaybeDecreaseHeight() {
  while (currentLevel_ > 0 && head_->Next(currentLevel_) == nullptr) {
    currentLevel_--;
  }
}

uint64_t CacheSkipList::EstimateStepCount(int level) { return (1ULL << (2 * level)); }


CacheSkipList::NodePtrType CacheSkipList::TraverseCount(
    CacheSkipList::NodePtrType x, size_t count, int* maxLevel) {
  assert(x != nullptr);
  assert(x != tail_);
  NodePtrType ptr = x;

  int level = 0;
  *maxLevel = 0;

  // go ascending
  while (ptr != tail_ && EstimateStepCount(level) <= count) {
    *maxLevel = std::max(*maxLevel, ptr->Height());
    // check if we can go to higher level
    while (level + 1 < ptr->Height() && EstimateStepCount(level + 1) <= count) {
      level++;
    }
    // level is the highest level we can step
    ptr = ptr->Next(level);
    count -= EstimateStepCount(level);
  }

  // descending
  while (ptr != tail_ && count > 0) {
    *maxLevel = std::max(*maxLevel, ptr->Height());
    // check if we need go to lower level
    while (level > 0 && EstimateStepCount(level) > count) {
      level--;
    }
    // level is the highest level we can step
    ptr = ptr->Next(level);
    count -= EstimateStepCount(level);
  }

  assert(ptr == tail_ || count == 0);
  return ptr;
}

CacheSkipList::NodePtrType CacheSkipList::DeleteRange(
    CacheSkipList::NodePtrType x, size_t count) {
  if (x == nullptr || count == 0) return nullptr;

  auto prev = x;
  x = x->Next(0);
  if (!prev->IsSentinel()) {
    InsertSentinel(prev);
  }

  std::vector<NodePtrType> preds(maxLevel_, nullptr);
  Find(x, preds);

  int maxLevel = 0;
  auto ptr = TraverseCount(x, count, &maxLevel);

  // ptr will not be removed
  if (ptr != nullptr && ptr->IsSentinel()) {
    ptr = ptr->Next(0);
  }

  auto next = FindNext(ptr, maxLevel);

  for (int i = next.size() - 1; i >= 0; i--) {
    preds[i]->Next(i) = next[i];
    assert(preds[i] == nullptr || cmpWrapper(preds[i], x->Key()) <= 0);
    assert(next[i] == nullptr || cmpWrapper(next[i], next[0]->Key()) >= 0);
    if (i > 1) {
      assert(next[i] == nullptr || next[i - 1] == nullptr ||
             cmpWrapper(next[i], next[i - 1]->Key()) >= 0);
    }
  }

  MaybeDecreaseHeight();
  DoDeleteRange(x, next[0]);
  return next[0];
}

void CacheSkipList::DoDeleteRange(CacheSkipList::NodePtrType begin,
                                  CacheSkipList::NodePtrType end) {
  NodePtrType ptr = begin;
  while (ptr != nullptr && ptr != end) {
    NodePtrType next = ptr->Next(0);
    assert(!ptr->deleted_);

    stats_.OnUnlinkNode(ptr);
    if (!ptr->IsSentinel()) {
      dbglprintf("delete key is %s \n", ptr->Key().c_str());
    }
    if (ptr->IsSentinel()) {
      CacheSkipList::DeallocateSentinel(ptr);
    } else {
      CacheSkipList::DeallocateNode(ptr);
    }
    ptr = next;
  }
}

bool CacheSkipList::Delete(const Slice& key) {
  std::vector<NodePtrType> preds(maxLevel_, nullptr);
  auto x = head_;

  for (int i = currentLevel_; i >= 0; i--) {
    while (x->Next(i) != nullptr && cmpWrapper(x->Next(i), key) < 0) {
      x = x->Next(i);
    }
    preds[i] = x;
  }

  x = x->Next(0);

  if (x != nullptr && cmpWrapper(x, key) == 0) {
    for (int i = 0; i <= currentLevel_; i++) {
      if (preds[i]->Next(i) != x) {
        break;
      }
      preds[i]->Next(i) = x->Next(i);
    }
    while (currentLevel_ > 0 && head_->Next(currentLevel_) == nullptr) {
      currentLevel_--;
    }
    stats_.OnUnlinkNode(x);
    return true;
  }
  return false;
}

int CacheSkipList::cmpWrapper(CacheSkipList::NodePtrType const node,
                              const Slice& rhs) const {
  if (node->IsSentinel()) {
    return -1;
  }
  return this->cmp_->Compare(node->Key(), rhs);
}

int CacheSkipList::RandomLevel() {
  int level = 1;
  while (rand() < RAND_MAX * CACHESKIPLIST_P && level < maxLevel_ - 1) {
    level++;
  }
  return level;
}

CacheSkipList::NodePtrType CacheSkipList::AllocateSentinel(int level) {
  auto ptr = new CacheSkipListNode(level);
  return ptr;
}
CacheSkipList::NodePtrType CacheSkipList::AllocateNode(const Slice& key,
                                                       const Slice& value,
                                                       int level) {
  auto ptr = new CacheSkipListNode(key, value, level);
  return ptr;
}

void CacheSkipList::DeallocateSentinel(CacheSkipList::NodePtrType ptr) {
  assert(ptr->IsSentinel());
  assert(!ptr->deleted_);
  ptr->deleted_ = true;
  delete ptr;
}

void CacheSkipList::DeallocateNode(NodePtrType ptr) {
  assert(!ptr->IsSentinel());
  assert(!ptr->deleted_);
  ptr->deleted_ = true;
  delete ptr;
}

std::unique_ptr<CacheSkipListIterator> CacheSkipList::NewIterator() {
  return std::make_unique<CacheSkipListIterator>(this);
}

std::unique_ptr<CacheSkipListIterator> CacheSkipList::NewIterator(
    CacheSkipList::NodePtrType current) {
  return std::make_unique<CacheSkipListIterator>(this, current);
}

bool CacheSkipList::M_DeleteSentinel(CacheSkipList::NodePtrType prev,
                                     CacheSkipList::NodePtrType x) {
  assert(x->IsSentinel());

  // TODO: marked

  auto p = [this, x, &prev]() { UnlinkSentinel(prev, x); };

  bool success = M_LockedExec(prev, x, p);
  if (success) {
    DeallocateSentinel(x);
  }
  return success;
}

void CacheSkipList::M_UpdateValue(CacheSkipList::NodePtrType x,
                                  const Slice& value) {
  {
    x->Lock();
    x->Value() = std::string(value.data(), value.size());
    x->Unlock();
    stats_.OnUpdateNode(x->Value(), value);
  }
}

static inline bool LockedCheck(CacheSkipList::NodePtrType pred,
                           CacheSkipList::NodePtrType succ, int level) {
  pred->Lock();
  // If pred marked or if the pred and succ change,
  // then abort and try again
  bool success = !(pred->marked_.load(std::memory_order_seq_cst)) &&
                 !(succ->marked_.load(std::memory_order_seq_cst)) &&
                 pred->Next(level) == succ;
  return success;
}

bool CacheSkipList::M_LockedExec(std::vector<CacheSkipList::NodePtrType>& preds,
                                 std::vector<CacheSkipList::NodePtrType>& succs,
                                 int maxLevel,
                                 const std::function<void()>& func) {
  bool success = true;
  int level = 0;

  for (; success && (level < maxLevel); level++) {
    success = LockedCheck(preds[level], succs[level], level);
  }

  if (success) {
    func();
  }

  for (level--; level >= 0; level--) {
    preds[level]->Unlock();
  }
  return success;
}

bool CacheSkipList::M_LockedExec(CacheSkipList::NodePtrType prev,
                                 std::vector<CacheSkipList::NodePtrType>& preds,
                                 std::vector<CacheSkipList::NodePtrType>& succs,
                                 int maxLevel,
                                 const std::function<void()>& func) {
  // Lock in reverse lexicographical order
  // 1. preds[0]
  // 2. prev
  // 3. preds[1...]

  int level = 0;

  bool success = LockedCheck(preds[level], succs[level], level);
  if (!success) goto unlock_preds0;
  success = LockedCheck(prev, preds[level], level);
  if (!success) goto unlock_prev;

  level++;
  for (; success && (level < maxLevel); level++) {
    success = LockedCheck(preds[level], succs[level], level);
  }

  if (success) {
    func();
  }

// unlock_all:
  for (level--; level >= 1; level--) {
    preds[level]->Unlock();
  }
unlock_prev:
  prev->Unlock();
unlock_preds0:
  preds[0]->Unlock();

  return success;
}
bool CacheSkipList::M_LockedExec(CacheSkipList::NodePtrType pred,
                                 CacheSkipList::NodePtrType succ,
                                 const std::function<void()>& func) {
  bool success = LockedCheck(pred, succ, 0);

  if (success) {
    func();
  }

  pred->Unlock();
  return success;
}

bool CacheSkipList::M_RetryFind(const Slice& key,
                                const RetryCallbackFuncType& pfFound,
                                const RetryCallbackFuncType& pfElse) {
  while (true) {
    NodePtrType prev;
    std::vector<NodePtrType> preds(maxLevel_, nullptr);
    std::vector<NodePtrType> succs(maxLevel_, nullptr);
    int top_level = RandomUpdateLevel();
    ResizeToLevel(preds, top_level);

    int found = -1;
    Find(key, prev, preds, succs, found);

    if (found != -1) {
      NodePtrType node_found = succs[found];
      if (!node_found->marked_) {
        // Case 1: If found and unmarked
        if (pfFound(prev, preds, succs)) {
          return true;
        } else {
          continue;
        }
      } else {
        // Case 2: If found and marked, wait and continue insert
        continue;
      }
    } else {
      // Case 3: If not found
      if (pfElse(prev, preds, succs)) {
        return true;
      } else {
        continue;
      }
    }
  }
  return false;
}


//bool CacheSkipList::M_Delete(const Slice& key) {
//  // Initialization
//  NodePtrType victim = nullptr;
//  bool is_marked = false;
//  int top_level = -1;
//
//  // Initialization of references of the predecessors and successors
//  std::vector<NodePtrType> preds(maxLevel_, nullptr);
//  std::vector<NodePtrType> succs(maxLevel_, nullptr);
//
//  // Keep trying to delete the element from the list. In case predecessors and
//  // successors are changed,this loop helps to try delete again
//  while (true) {
//    // Find the predecessors and successors of where the key to be deleted
//    int found = -1;
//    NodePtrType prev;
//    NodePtrType x = Find(key, prev, preds, succs, found);
//
//    // If found, select the node to delete. else return
//    if (found != -1) {
//      victim = succs[found];
//    }
//
//    if (victim == nullptr) {
//      return false;  // Node not found, exit
//    }
//
//    // If node not found and the node to be deleted is fully linked
//    // and not marked return
//    if (is_marked | (found != -1 &&
//                     (victim->fully_linked_ &&
//                      victim->top_level_ - 1 == found && !(victim->marked_)))) {
//      // if not marked, then we lock the node and mark the node to delete
//      if (!is_marked) {
//        top_level = victim->top_level_;
//        victim->lock();
//        if (victim->marked_) {
//          victim->unlock();
//          return false;
//        }
//        victim->marked_ = true;
//        is_marked = true;
//      }
//
//      // Store all the Nodes which lock we acquire in a map
//      // Map used so that we don't try to acquire lock to a Node we have
//      // already acquired This may happen when we have the same predecessor at
//      // different levels
//      std::map<NodePtrType, int> locked_nodes;
//
//      // Traverse the skip list and try to acquire the lock of predecessor at
//      // every level
//      try {
//        NodePtrType pred;
//
//        // Used to check if the predecessors are not marked for delete
//        // and if the predecessors next is the node we are trying to delete
//        // or if it is changed
//        bool valid = true;
//
//        for (int level = 0; valid && (level < top_level); level++) {
//          pred = preds[level];
//
//          // If not already acquired lock, then acquire the lock
//          if (!(locked_nodes.count(pred))) {
//            pred->lock();
//            locked_nodes.insert(std::make_pair(pred, 1));
//          }
//
//          // If predecessor marked or if the predecessor's next has changed,
//          // the abort and try again
//          valid = !(pred->marked_) && pred->Next(level) == victim;
//        }
//
//        // Conditions are not met, release locks, abort and try again.
//        if (!valid) {
//          for (auto const& p : locked_nodes) {
//            p.first->unlock();
//          }
//          continue;
//        }
//
//        // All conditions satisfied,delete the Node and link them to the
//        // successors appropriately
//        for (int level = top_level - 1; level >= 0; level--) {
//          preds[level]->Next(level) = victim->Next(level);
//        }
//
//        victim->unlock();
//
//        while (currentLevel_ > 0 && head_->Next(currentLevel_) == nullptr) {
//          // TODO currentLevel_ is not used currentLevel_--;
//        }
//
//        stats_.OnUnlinkNode(victim);
//        delete victim;
//
//        // Delete is completed, release the locks held.
//        for (auto const& p : locked_nodes) {
//          p.first->unlock();
//        }
//
//        return true;
//      } catch (const std::exception& e) {
//        // If any exception occurs during the above delete, release locks of
//        // the held nodes and try again.
//        for (auto const& p : locked_nodes) {
//          p.first->unlock();
//        }
//      }
//    } else {
//      return false;
//    }
//  }
//}
}  // namespace rocksdb