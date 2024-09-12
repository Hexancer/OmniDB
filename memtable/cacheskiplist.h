#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <ostream>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#include "rocksdb/comparator.h"
#include "rocksdb/debug.h"
#include "rocksdb/perf_data_client.h"

namespace rocksdb {

#define CACHESKIPLIST_MAXLEVEL 8   // 最大层级数
#define CACHESKIPLIST_P 0.25       // 跳表概率因子
#define SENTINEL ((char*)0xdeadbeefdeadbeef)
#define TAIL "\xff\xff\xff\xff\xff\xff\xff\xffTAIL"
#define SENTINEL_STR "sentinel"

/**
 * CacheSkipListNodeStats
 */
struct  CacheSkipListNodeStats {
  int access_count_ = 0;
  void OnAccess() { access_count_++; }
};

struct CacheSkipListEAStats {
  uint32_t access_count_{};
  uint32_t length_{};

  void OnLinkNode() { length_++; }
  void OnUnlinkNode() { length_--; }
  void OnAccess() { access_count_++; }

  void IncLength(uint32_t inc) { length_ += inc; }
  void DecLength(uint32_t dec) { assert(length_ >= dec); length_ -= dec; }
};


/**
 * CacheSkipListNode
 */
class CacheSkipListNode {
 public:
  typedef CacheSkipListNode* NodePtrType;
  friend std::ostream& operator<<(std::ostream& os,
                                  const CacheSkipListNode& node);

  NodePtrType next0_;
  union {
    struct {
      char* sentinel_;
      CacheSkipListEAStats ea_stats_;
    } se_;
    struct {
      std::string key_;
      std::string value_;
    } kv_;
  };
  std::vector<NodePtrType> next_;
  bool deleted_ = false;
  CacheSkipListNodeStats stats_{};

  std::recursive_mutex mutex_;
  // Atomic to be marked if this Node is being deleted
  std::atomic<bool> marked_ = {false};
  // Atomic to indicate Node is completely linked to  predecessors and sucessors
  std::atomic<bool> fully_linked_ = {false};

  explicit CacheSkipListNode(int level)
      : next0_(nullptr),
        se_{.sentinel_ = SENTINEL, .ea_stats_{}},
        next_(level - 1, nullptr) {}
  CacheSkipListNode(const Slice& key, const Slice& value, int level)
      : next0_(nullptr),
        kv_{.key_ = {key.data(), key.size()},
            .value_ = {value.data(), value.size()}},
        next_(level - 1, nullptr) {}
  ~CacheSkipListNode() {
    typedef std::string String;
    if (!IsSentinel()) {
      kv_.key_.~String();
      kv_.value_.~String();
    }
  }

  void Lock() { mutex_.lock(); }
  void Unlock() { mutex_.unlock(); }

  bool IsSentinel() const { return this->se_.sentinel_ == SENTINEL; }

  __always_inline NodePtrType& Next(int i) {
    if (i == 0) {
      return next0_;
    } else {
      return next_[i - 1];
    }
  }
  std::string& Key() { return this->kv_.key_; }
  std::string& Value() { return this->kv_.value_; }
  const std::string& Key() const { return this->kv_.key_; }
  const std::string& Value() const { return this->kv_.value_; }
  int Height() const { return (int)(this->next_.size() + 1); }
};  // class CacheSkipListNode


/**
 * CacheSkipListStats
 */
struct CacheSkipListStats {
  typedef CacheSkipListNode* NodePtrType;

#define PERF_COUNTER_PREFIX "/oc/skiplist/"

  PERFCOUNTER_DEF_T(std::atomic<size_t>, PERF_COUNTER_PREFIX, length_);
  PERFCOUNTER_DEF_T(std::atomic<size_t>, PERF_COUNTER_PREFIX, keySize_);
  PERFCOUNTER_DEF_T(std::atomic<size_t>, PERF_COUNTER_PREFIX, valueSize_);
  PERFCOUNTER_DEF(PERF_COUNTER_PREFIX, insertCount_);
  PERFCOUNTER_DEF(PERF_COUNTER_PREFIX, appendCount_);
  PERFCOUNTER_DEF(PERF_COUNTER_PREFIX, evictCount_);
  PERFCOUNTER_DEF(PERF_COUNTER_PREFIX, evictLength_);

  PERFCOUNTER_DEF(PERF_COUNTER_PREFIX, sentinelCount_);

  size_t levelLength_[CACHESKIPLIST_MAXLEVEL]{};

  PERFCOUNTER_DEF(PERF_COUNTER_PREFIX, findCount_);
  PERFCOUNTER_DEF(PERF_COUNTER_PREFIX, findIterCount_);

  explicit CacheSkipListStats();

  void OnLinkNode(NodePtrType x) {
    if (!x->IsSentinel()) {
      length_++;
      keySize_ += x->Key().size();
      valueSize_ += x->Value().size();
    } else {
      sentinelCount_++;
    }
    levelLength_[x->Height() - 1]++;
  }
  void OnUpdateNode(const std::string &oldValue, const std::string &newValue) {
    valueSize_ += newValue.size() - oldValue.size();
  }
  void OnUpdateNode(const std::string &oldValue, const Slice &newValue) {
    valueSize_ += newValue.size() - oldValue.size();
  }
  void OnUnlinkNode(NodePtrType x) {
    if (!x->IsSentinel()) {
      length_--;
      keySize_ -= x->Key().size();
      valueSize_ -= x->Value().size();
    } else {
      sentinelCount_--;
    }
    levelLength_[x->Height() - 1]--;
  }
  void OnInsert() { insertCount_++; }
  void OnAppend() { appendCount_++; }
  void OnEvict() { evictCount_++; }

  void Dump() {
    auto &client = PerfDataClient::GetPerfDataClient();
    client.DumpMetric(PERF_COUNTER_PREFIX);
  }
};


class CacheSkipListIterator;


/**
 * CacheSkipList
 */
class CacheSkipList {
 public:
  /**
   * Type declarations
   */
  typedef CacheSkipListNode* NodePtrType;

 private:
  friend class CacheSkipListIterator;

 public:
  explicit CacheSkipList(int maxLevel, const Comparator* cmp, size_t maxsize);
  ~CacheSkipList();
  void DestroyAllNodes();

  /**
   * Linking Routines
   */
  void LinkNode(NodePtrType newNode, int level,
                const std::vector<NodePtrType>& preds);
  void LinkNode(NodePtrType newNode, NodePtrType pred, NodePtrType succ);
  void LinkNode(CacheSkipList::NodePtrType newNode, int level,
                const std::vector<NodePtrType>& preds,
                const std::vector<NodePtrType>& succs);
  void UnlinkNode(NodePtrType x, std::vector<NodePtrType> preds);

  /**
   * Sentinel Routines
   */
  void InsertSentinel(NodePtrType node);
  void UnlinkSentinel(NodePtrType prev, NodePtrType node);
  void DeleteSentinel(NodePtrType prev, NodePtrType node);

  bool M_DeleteSentinel(NodePtrType prev, NodePtrType node);

  /**
   * Debug Routines
   */
  void __attribute__((noinline)) PrintAllNodes() const;
  std::vector<std::string> DumpAllNodes() const;
  std::vector<std::string> DumpAllNodesNoSentinel() const;

  /**
   * Find Routines
   * w/o & w/ update
   */
  // Find the largest (entry or sentinel) < key
  NodePtrType Find(const Slice& key);
  // Find the largest (entry or sentinel) < key, with update locs
  NodePtrType Find(const Slice& key, std::vector<NodePtrType>& preds);
  // Find the largest (entry or sentinel) < key, with update locs
  void Find(NodePtrType ptr, std::vector<NodePtrType>& preds);
  // This func will climb up the skiplist again to get the next vector
  std::vector<NodePtrType> FindNext(NodePtrType ptr, int maxFillLevel);
  // Find the largest (entry or sentinel) < key, with preds and succs
  NodePtrType Find(const Slice& key, NodePtrType& prev,
                   std::vector<NodePtrType>& preds,
                   std::vector<NodePtrType>& succs, int& found);

  /**
   * Seek Routines
   * w/o & w/ update
   */
  // Find the first entry for key
  //    If hit inside [closed ranges], gives a valid entry >= key
  //    Else gives a invalid sentinel < key
  NodePtrType Seek(const Slice& key);
  NodePtrType Seek(const Slice& key, std::vector<NodePtrType>& preds);

  /**
   * Evict Routines
   */
  bool ShouldEvict() const;
  void Evict();

  /**
   * Insert Routines
   */
  std::unique_ptr<CacheSkipListIterator> Insert(const Slice& key,
                                                const Slice& value);
  std::unique_ptr<CacheSkipListIterator> Append(const Slice& key,
                                                const Slice& value);
  std::unique_ptr<CacheSkipListIterator> M_Insert(const Slice& key,
                                             const Slice& value);
  std::unique_ptr<CacheSkipListIterator> M_Append(const Slice& key,
                                             const Slice& value);

  /**
   * Lock Routines
   */
  void M_UpdateValue(NodePtrType x, const Slice& value);

  static bool M_LockedExec(std::vector<NodePtrType>& preds,
                           std::vector<NodePtrType>& succs,
                           int maxLevel, const std::function<void()>& func);
  static bool M_LockedExec(NodePtrType prev,
                           std::vector<NodePtrType>& preds,
                           std::vector<NodePtrType>& succs,
                           int maxLevel, const std::function<void()>& func);
  static bool M_LockedExec(NodePtrType pred,
                           NodePtrType succ,
                           const std::function<void()>& func);

  typedef std::function<bool(NodePtrType, std::vector<NodePtrType>&,
                             std::vector<NodePtrType>&)>
      RetryCallbackFuncType;
  bool M_RetryFind(const Slice& key, const RetryCallbackFuncType& pfFound,
                   const RetryCallbackFuncType& pfElse);

  /**
   * Height util
   */
  int RandomUpdateLevel();
  void ResizeToLevel(std::vector<NodePtrType>& preds, int level);
  void MaybeDecreaseHeight();

  /**
   * Delete util
   */
  uint64_t EstimateStepCount(int level);
  void InsertSentinelBefore(NodePtrType node);
  NodePtrType TraverseCount(NodePtrType x, size_t count, int* maxLevel);
  NodePtrType DeleteRange(NodePtrType x, size_t count);
  void DoDeleteRange(NodePtrType begin,
                     NodePtrType end);
  bool Delete(const Slice& key);
  bool M_Delete(const Slice& key);



  std::unique_ptr<CacheSkipListIterator> NewIterator();
  std::unique_ptr<CacheSkipListIterator> NewIterator(NodePtrType current);

  static NodePtrType AllocateSentinel(int level);
  static NodePtrType AllocateNode(const Slice& key, const Slice& value, int level);
  static void DeallocateSentinel(NodePtrType ptr);
  static void DeallocateNode(NodePtrType ptr);

 private:
  inline int cmpWrapper(const NodePtrType node, const Slice& rhs) const;

  int RandomLevel();

  NodePtrType head_;
  int maxLevel_;
  std::atomic<int> currentLevel_;
  NodePtrType tail_;
  CacheSkipListStats stats_;
  const Comparator* cmp_;
  size_t maxSize_ = 50000;
};  // class CacheSkipList


/**
 * CacheSkipListIterator
 */
class CacheSkipListIterator {
  typedef CacheSkipListNode* NodePtrType;

 public:
  explicit CacheSkipListIterator(CacheSkipList* skiplist)
      : skiplist_(skiplist), current_(skiplist->head_->Next(0)) {}
  explicit CacheSkipListIterator(CacheSkipList* skiplist, NodePtrType current)
      : skiplist_(skiplist), current_(std::move(current)) {}

  void Seek(const Slice& target) { current_ = skiplist_->Seek(target); }
  void SeekToFirst() { current_ = skiplist_->head_->Next(0); }
  void SeekToLast() { current_ = skiplist_->tail_; }
  void Next() {
    if (current_ != nullptr) {
      current_->stats_.OnAccess();

      current_ = current_->Next(0);
    }
  }
  bool Valid() const {
    return current_ != nullptr && !current_->IsSentinel();
  }
  Slice key() const {
    return current_ != nullptr ? Slice(current_->Key()) : Slice();
  }
  Slice value() const {
    return current_ != nullptr ? Slice(current_->Value()) : Slice();
  }

 private:
  CacheSkipList* skiplist_;
  NodePtrType current_;
};

}  // namespace rocksdb
