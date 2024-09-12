#ifndef ROCKSDB_FOLLYSKIPLIST_H
#define ROCKSDB_FOLLYSKIPLIST_H

#include "myfolly/ConcurrentSkipList.h"
#include "folly/concurrency/container/LockFreeRingBuffer.h"
#include "rocksdb/comparator.h"
#include "rocksdb/slice.h"
#include "rocksdb/db.h"
#include "utilities/persistent_cache/lrulist.h"
#include "memtable/es.h"
#include "cache/lru_cache.h"

#define SENTINEL ((char*)0xdeadbeefdeadbeef)
#define SENTINEL_STR "sentinel"
#define TAIL "\xff\xff\xff\xff\xff\xff\xff\xffTAIL"

namespace rocksdb {

struct FollyKVComparator;

/**
 * FollyKV
 */
struct FollyKV {
  friend std::ostream& operator<<(std::ostream& os, const FollyKV& node) {
    os << "NodePtr: " << &node << " Key: " << node.Key()
       << " Value: " << node.Value();
    if (node.sentinel_) {
      os << "(Sentinel)";
    }
    return os;
  }
  std::string key_;
  std::string value_;
  bool sentinel_;
  bool dirty_;

  FollyKV() : key_(), value_(), sentinel_(true), dirty_(false) {}
  FollyKV(const Slice& key, const Slice& value)
      : key_(key.data(), key.size()),
        value_(value.data(), value.size()),
        sentinel_(false), dirty_(false) {}
  FollyKV(const Slice& key, const Slice& value, bool sentinel)
      : key_(key.data(), key.size()),
        value_(value.data(), value.size()),
        sentinel_(sentinel), dirty_(false) {}
  FollyKV(const std::string& key, const std::string& value, bool sentinel)
      : key_(key), value_(value), sentinel_(sentinel), dirty_(false) {}

  bool IsSentinel() const { return this->sentinel_; }

  std::string& Key() { return this->key_; }
  std::string& Value() { return this->value_; }
  const std::string& Key() const { return this->key_; }
  const std::string& Value() const { return this->value_; }

  // Copy ctors
  FollyKV(const FollyKV& other)
      : key_(other.key_), value_(other.value_), sentinel_(other.sentinel_), dirty_(other.dirty_) {}
  // copy assignment
  FollyKV& operator=(const FollyKV& other) {
    if (this != &other) {
      key_ = other.key_;
      value_ = other.value_;
      sentinel_ = other.sentinel_;
      dirty_ = other.dirty_;
    }
    return *this;
  }
  // move ctor
  FollyKV(FollyKV&& other) noexcept
      : key_(std::move(other.key_)),
        value_(std::move(other.value_)),
        sentinel_(other.sentinel_), dirty_(other.dirty_) {}
  // move assignment
  FollyKV& operator=(FollyKV&& other) noexcept {
    if (this != &other) {
      key_ = std::move(other.key_);
      value_ = std::move(other.value_);
      sentinel_ = other.sentinel_;
      dirty_ = other.dirty_;
    }
    return *this;
  }

};  // struct FollyKV

extern DB* pDBImpl;


template <class T>
struct RRList {
  typedef folly::ConcurrentSkipList<FollyKV, FollyKVComparator>::NodeType NodeType;
 public:
  RRList(NodeType* head) : head_(head), ptr_(head) {}

  NodeType* head_;
  NodeType* ptr_;
  inline void Push(T* const t) { }
  inline void Unlink(T* const t) { }

  inline T* Pop() {
    if (ptr_ == nullptr) {
      ptr_ = head_;
    }
    if (ptr_ == head_) {
      ptr_ = ptr_->next();
    }
    NodeType* ret = ptr_;
    ptr_ = ptr_->next();

    static uint64_t counter = 0;
    if (counter ++ % 1048576 == 0) {
      fprintf(stderr, "poped for %lu times\n", counter);
    }

    return ret;
  }

  inline void Touch(T* const t) {}

  inline bool IsEmpty() const { return false; }

};
extern LRUList<folly::ConcurrentSkipList<FollyKV, FollyKVComparator>::NodeType> *lru_list;

/**
 * FollyKVComparator
 */
struct FollyKVComparator {
 public:
  bool operator()(const FollyKV& node, const FollyKV& data) const {
    return node.Key() < data.Key();
  }
};


template <typename T>
class MyAllocator {
 private:
  using Self = MyAllocator<T>;

 public:
  using value_type = T;

  constexpr MyAllocator() = default;

  constexpr MyAllocator(MyAllocator const&) = default;

  template <typename U, std::enable_if_t<!std::is_same<U, T>::value, int> = 0>
  constexpr MyAllocator(MyAllocator<U> const&) noexcept {}

  T* allocate(size_t count) {
    auto const p = std::malloc(sizeof(T) * count);
    return static_cast<T*>(p);
  }
  void deallocate(T* p, size_t count) {
    sizedFree(p, count * sizeof(T));
  }

  friend bool operator==(Self const&, Self const&) noexcept { return true; }
  friend bool operator!=(Self const&, Self const&) noexcept { return false; }
};

extern Cache::CacheItemHelper helper1_wos;



/**
 * FollySkipList
 */
struct FollySkipList {
  typedef folly::ConcurrentSkipList<FollyKV, FollyKVComparator> SkipListType;
  typedef SkipListType::NodeType NodeType;
  typedef SkipListType::Accessor Accessor;

    struct Iterator {
      typedef FollySkipList::NodeType NodeType;
      typedef FollySkipList::SkipListType SkipListType;
      typedef FollySkipList::Accessor Accessor;

     public:
      Iterator(std::shared_ptr<SkipListType> skipList, NodeType* ptr, FollySkipList* fsl)
          : accessor_(std::move(skipList)), ptr_(ptr), valid_(true), fsl_(fsl) {}

      Iterator(const Accessor& accessor, NodeType* ptr, FollySkipList* fsl)
          : accessor_(accessor), ptr_(ptr), valid_(true), fsl_(fsl) {

      }

      void Next() {
        if (ptr_ && valid_) {
          valid_ = !ptr_->data().IsSentinel();
          ptr_ = ptr_->next();
          lru_list->Touch(ptr_);
        }
      }

      void Seek(const Slice& key) {
        FollyKV node(key, std::string(""));
        ptr_ = fsl_->Seek(key);
        valid_ = ptr_->data().key_ == key;
      }

      bool Valid() const { return valid_; }
      std::string& Key() const { return ptr_->data().Key(); }
      std::string& Value() const { return ptr_->data().Value(); }

     private:
      Accessor accessor_;
      NodeType* ptr_;
      bool valid_;
      FollySkipList *fsl_;
    };

  // don't hold a accessor, make GC possible
  std::shared_ptr<SkipListType> skiplist_;
  size_t  maxSize_;
//  std::shared_ptr<Cache> cache =  NewLRUCache(32 * 1048576);

  folly::LockFreeRingBuffer<WriteBatch*> ringbuf_ =
      folly::LockFreeRingBuffer<WriteBatch*>(32);

  FollySkipList(int maxLevel, const Comparator* cmp, size_t maxsize)
      : maxSize_(maxsize) {
    FollyKV headKV(std::string(""), std::string(), true);
    FollyKV tailKV(std::string(TAIL), std::string(), true);

    skiplist_ = SkipListType::createInstance(headKV, maxLevel);

    auto headNode = skiplist_.get()->head_.load();
    auto tailNode = skiplist_->createNode(tailKV, 1);
    headNode->setSkip(0, tailNode);
    tailNode->setBack(headNode);

    lru_list = new LRUList<NodeType>();

    // create a thread to run LRUEvict
    std::thread([this]() {
      while (true) {
        WriteOptions wopt;
        auto cursor = ringbuf_.currentHead();
        WriteBatch *pBatch = nullptr;
        ringbuf_.waitAndTryRead(pBatch, cursor);
        assert(pBatch);
        pDBImpl->Write(wopt, pBatch);
      }
    }).detach();

  }

  NodeType* Seek(const Slice& key) {
    FollyKV node(key, std::string(""));

    // Point Hash
//    auto handle = static_cast<LRUHandle*>(cache->Lookup(key));
//    if (handle != nullptr) {
//      NodeType* result = static_cast<NodeType*>(handle->value);
//      cache->Release(handle);
//    }

    // Skip List
    NodeType* pNode = skiplist_->seek(node);
    // Evict
    if (pNode != skiplist_.get()->head_) {
      lru_list->Touch(pNode);
    }
    // Refill Point Hash
//    cache->Insert(key, pNode, &helper1_wos, sizeof(pNode));
    return pNode;
  }

  bool ShouldEvict() { return skiplist_->size() * 1000 > maxSize_; }

  void EvictWrite(const FollyKV& data) {
    pDBImpl->Put_OC_Delete(data.Key(), data.Value());
  }

  void LRUEvict() {
    ColumnFamilyHandle* default_cf_ = pDBImpl->DefaultColumnFamily();
//    Cache* pCache = cache.get();
    WriteBatch batch(1152 * 1024, 0 /* max_bytes */, 0, 0 /* default_cf_ts_sz */);
    // Pre-allocate size of write batch conservatively.
    // 8 bytes are taken by header, 4 bytes for count, 1 byte for type,
    // and we allocate 11 extra bytes for key length, as well as value length.
//    WriteBatch* batch = nullptr;
    for (int i = 0; i < 1024; ++i) {
      if (lru_list->IsEmpty()) {
        break;
      }
      auto victim = lru_list->Pop();

      victim->back()->data().sentinel_ = true;

      FollyKV& data = victim->data();
      if (data.dirty_) {
//        if (batch == nullptr) {
//          batch = new WriteBatch(1152 * 2, 0 /* max_bytes */, 0,
//                                 0 /* default_cf_ts_sz */);
//        }
        std::string wkey;
        std::string wval;
        wkey.assign(data.Key().data(), data.Key().size());
        wval.assign(data.Value().data(), data.Value().size());
        batch.Put(default_cf_, wkey, wval);
      }
//      pCache->Erase(data.Key());
      skiplist_.get()->remove(data);
    }
    WriteOptions wopt;
    wopt.disableWAL =true;
    pDBImpl->Write(wopt, &batch);
    //    if (batch) {
//      ringbuf_.write(batch);
//    }
  }

  std::unique_ptr<Iterator> Insert(const Slice& key, const Slice& value) {
    return doInsert(key, value, false);
  }

  std::unique_ptr<Iterator> Append(const Slice& key, const Slice& value) {
    return doInsert(key, value, true);
  }

  std::unique_ptr<Iterator> doInsert(const Slice& key, const Slice& value, bool doAppend) {
    if (ShouldEvict()) {
      LRUEvict();
      doAppend = false;
    }
    FollyKV node{key, value, true};
    SkipListType::Accessor accessor(skiplist_);
    auto [p, added] = skiplist_->addOrGetData(node, doAppend);
    if (!added) {
      p->data().Value() = node.Value();
      p->data().dirty_ = true;
    } else {
      assert(p->data().Key() != "");
      lru_list->Push(p);
    }
    lru_list->Touch(p);

    return std::make_unique<Iterator>(skiplist_, p, this);
  }


  NodeType* Find(const Slice& key) {
    FollyKV node(key, std::string(""));
    SkipListType::Accessor accessor(skiplist_);
    NodeType* pNode = skiplist_->lower_bound(node);
    if (pNode != skiplist_.get()->head_) {
      lru_list->Touch(pNode);
    }
    return pNode;
  }

  std::unique_ptr<Iterator> NewIterator() {
    return std::make_unique<Iterator>(skiplist_, nullptr, this);
  }

  std::vector<std::string> DumpAllNodes() const {
    SkipListType::Accessor accessor(skiplist_);
    std::vector<std::string> xs;
    auto x = skiplist_->head_.load();
    while (x != nullptr) {
      //      if (x->data().IsSentinel()) {
      //        xs.emplace_back(SENTINEL_STR);
      //      } else {
      xs.push_back(x->data().Key());
      //      }
      x = x->next();
    }
    return xs;
  }
};

}  // namespace rocksdb

#endif  // ROCKSDB_FOLLYSKIPLIST_H
