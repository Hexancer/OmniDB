////  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
////  This source code is licensed under both the GPLv2 (found in the
////  COPYING file in the root directory) and Apache 2.0 License
////  (found in the LICENSE.Apache file in the root directory).
////
//// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
//// Use of this source code is governed by a BSD-style license that can be
//// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
//#include "memtable/cacheskiplist.h"
//
//#include <set>
//#include <unordered_set>
//
//#include "memory/concurrent_arena.h"
//#include "rocksdb/env.h"
//#include "test_util/testharness.h"
//#include "util/hash.h"
//#include "util/random.h"
//
//namespace ROCKSDB_NAMESPACE {
//
#include <iostream>
#include <unordered_set>
#include <map>
#include "test_util/testharness.h"
#include "gtest/gtest.h"
#include "memtable/rangeskiplist.h"
#include "rocksdb/slice.h"

namespace ROCKSDB_NAMESPACE {

// 使用 std::string 作为 Key 类型
using Key = Slice;

// 定义测试用的 Comparator
struct TestComparator {
  int Compare(const Key& a, const Key& b) const {
    return a.compare(b);
  }
};

// 测试类
class RangeSkipListTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // 初始化跳表
    skiplist = new RangeSkipList<TestComparator>(RANGESKIPLIST_MAXLEVEL, &cmp);
  }

  void TearDown() override {
    // 清理跳表
    delete skiplist;
  }

  RangeSkipList<TestComparator>* skiplist;
  TestComparator cmp;
};

TEST_F(RangeSkipListTest, BatchInsertSeekDelete) {
  // 批量插入
  skiplist->Insert("10", "20");

  skiplist->Insert("30", "40");
  skiplist->Insert("30", "39");
  skiplist->Insert("35", "38");
  skiplist->Insert("34", "37");
  skiplist->Insert("40", "50");
  skiplist->Insert("41", "59");
  skiplist->Insert("32", "55");
  skiplist->Insert("15", "33");
  skiplist->Insert("50", "60");
  skiplist->Insert("50", "60");
  skiplist->Insert("900", "960");
  skiplist->Insert("950", "9960");
  skiplist->Insert("988", "988");
  skiplist->Insert("010", "010");
  ASSERT_TRUE(skiplist->Hit("35"));
  ASSERT_FALSE(skiplist->Hit("9"));

  ASSERT_TRUE(skiplist->Hit("9959"));
//  // 创建迭代器并查找（seek）到某个值
  auto iter = skiplist->NewIterator();
//  iter->Seek("30");
//  ASSERT_TRUE(iter->Valid());
//  ASSERT_EQ(iter->rangeLeft(), "30");
//  ASSERT_EQ(iter->rangeRight(), "40");

//  // 删除找到的键
//  bool deleted = skiplist->Delete("30", "40");
//  ASSERT_TRUE(deleted);

//  // 再次查找删除的键，应该不存在
//  iter->Seek("30");
//  ASSERT_FALSE(iter->Valid());

  // 继续插入新的值
  skiplist->Insert("60", "70");
  skiplist->Insert("70", "80");

  // 再次删除某个键
//  deleted = skiplist->Delete("40", "50");
//  ASSERT_TRUE(deleted);

//  // 验证插入和删除的结果
//  iter->SeekToFirst();  // 从头开始
//  std::unordered_set<std::string> expected_keys = {"10", "20", "50", "60", "70"};
//  while (iter->Valid()) {
//    std::string key = iter->rangeLeft();
//    ASSERT_TRUE(expected_keys.find(key) != expected_keys.end());
//    expected_keys.erase(key);
//    iter->Next();
//  }

//  for (auto a : expected_keys) {
//    std::cout << "EK: " << a << std::endl;
//  }
//  ASSERT_TRUE(expected_keys.empty());

  // 遍历跳表中的所有记录并按顺序打印
  std::cout << "SkipList contents:" << std::endl;
  iter->SeekToFirst();
  while (iter->Valid()) {
    std::cout << "Left: " << iter->rangeLeft() << ", Right: " << iter->rangeRight() << std::endl;
    iter->Next();
  }
}
TEST_F(RangeSkipListTest, LastHitAndOverStep) {
  // 插入一些区间
  skiplist->Insert("10", "20");
  skiplist->Insert("30", "40");
  skiplist->Insert("50", "60");

  // 测试 lastHit_
  bool hit = skiplist->Hit("35");
  ASSERT_TRUE(hit);
//  ASSERT_EQ(skiplist->GetLastHit()->left_, "30");
//  ASSERT_EQ(skiplist->GetLastHit()->right_, "40");

  hit = skiplist->Hit("15");
  ASSERT_TRUE(hit);
//  ASSERT_EQ(skiplist->GetLastHit()->left_, "10");
//  ASSERT_EQ(skiplist->GetLastHit()->right_, "20");

  hit = skiplist->Hit("25");
  ASSERT_FALSE(hit);

  // 测试 OverStep
  skiplist->Hit("35");  // lastHit_ should be [30, 40]
  ASSERT_TRUE(skiplist->Overstep("40"));  // Still within [30, 40]

  skiplist->Hit("55");  // lastHit_ should be [50, 60]
  ASSERT_FALSE(skiplist->Overstep("55"));  // Still within [50, 60]

  skiplist->Hit("45");  // Not in any range
  ASSERT_TRUE(skiplist->Overstep("45"));  // Overstep as no valid range
}
}  // namespace ROCKSDB_NAMESPACE

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
//// Our test skip list stores 8-byte unsigned integers
//using Key = uint64_t;
//
//static const char* Encode(const uint64_t* key) {
//  return reinterpret_cast<const char*>(key);
//}
//
//static Key Decode(const char* key) {
//  Key rv;
//  memcpy(&rv, key, sizeof(Key));
//  return rv;
//}
//
//struct TestComparator {
//  using DecodedType = Key;
//
//  static DecodedType decode_key(const char* b) { return Decode(b); }
//
//  int operator()(const char* a, const char* b) const {
//    if (Decode(a) < Decode(b)) {
//      return -1;
//    } else if (Decode(a) > Decode(b)) {
//      return +1;
//    } else {
//      return 0;
//    }
//  }
//
//  int operator()(const char* a, const DecodedType b) const {
//    if (Decode(a) < b) {
//      return -1;
//    } else if (Decode(a) > b) {
//      return +1;
//    } else {
//      return 0;
//    }
//  }
//};
//
//using TestCacheSkipList = CacheSkipList<TestComparator>;
//
//class CacheSkipListTest : public testing::Test {
// public:
//  void Insert(TestCacheSkipList* list, Key key) {
//    char* buf = list->AllocateKey(sizeof(Key));
//    memcpy(buf, &key, sizeof(Key));
//    list->Insert(buf);
//    keys_.insert(key);
//  }
//
//  bool InsertWithHint(TestCacheSkipList* list, Key key, void** hint) {
//    char* buf = list->AllocateKey(sizeof(Key));
//    memcpy(buf, &key, sizeof(Key));
//    bool res = list->InsertWithHint(buf, hint);
//    keys_.insert(key);
//    return res;
//  }
//
//  void InsertAndUpdate(TestCacheSkipList* list, Key key, Key new_value) {
//    // Insert the original key
//    char* buf1 = list->AllocateKey(sizeof(Key));
//    memcpy(buf1, &key, sizeof(Key));
//    list->Insert(buf1);
//
//    // Verify the original key is present
//    TestCacheSkipList::Iterator iter(list);
//    iter.Seek(Encode(&key));
//    ASSERT_TRUE(iter.Valid());
//    ASSERT_EQ(key, Decode(iter.key()));
//
//    // Insert the new value for the same key
//    char* buf2 = list->AllocateKey(sizeof(Key));
//    memcpy(buf2, &new_value, sizeof(Key));
//    list->Insert(buf2);
//
//    // Verify the updated key is present
//    iter.Seek(Encode(&new_value));
//    ASSERT_TRUE(iter.Valid());
//    ASSERT_EQ(new_value, Decode(iter.key()));
//
//    // Ensure only one key (updated key) is present
//    iter.Seek(Encode(&key));
//    int count = 0;
//    while (iter.Valid() && Decode(iter.key()) == key) {
//      count++;
//      iter.Next();
//    }
//    ASSERT_EQ(count, 1);  // Ensure there's only one instance of the key
//  }
//
//  // 验证键值对是否存在
//  void VerifyKeyExists(CacheSkipList<TestComparator>* list, Key key) {
//    CacheSkipList<TestComparator>::Iterator iter(list);
//    iter.Seek(Encode(&key));
//    ASSERT_TRUE(iter.Valid());
//    ASSERT_EQ(key, Decode(iter.key()));
//  }
//
//  // 验证键值对是否不存在
//  void VerifyKeyNotExists(CacheSkipList<TestComparator>* list, Key key) {
//    CacheSkipList<TestComparator>::Iterator iter(list);
//    iter.Seek(Encode(&key));
//    ASSERT_FALSE(iter.Valid());
//  }
//
//  void Validate(TestCacheSkipList* list) {
//    // Check keys exist.
//    for (Key key : keys_) {
//      ASSERT_TRUE(list->Contains(Encode(&key)));
//    }
//    // Iterate over the list, make sure keys appears in order and no extra
//    // keys exist.
//    TestCacheSkipList::Iterator iter(list);
//    ASSERT_FALSE(iter.Valid());
//    Key zero = 0;
//    iter.Seek(Encode(&zero));
//    for (Key key : keys_) {
//      ASSERT_TRUE(iter.Valid());
//      ASSERT_EQ(key, Decode(iter.key()));
//      iter.Next();
//    }
//    ASSERT_FALSE(iter.Valid());
//    // Validate the list is well-formed.
//    list->TEST_Validate();
//  }
//
// private:
//  std::set<Key> keys_;
//};
//
//TEST_F(CacheSkipListTest, InsertAndUpdateTest) {
//  Arena arena;
//  TestComparator cmp;
//  CacheSkipList<TestComparator> list(cmp, &arena);
//
//  Key original_key = 10;
//  Key updated_key = 20;
//
//  // Insert original key and then update it
//  InsertAndUpdate(&list, original_key, updated_key);
//
//  // Validate the skip list
////  Validate(&list);
//}
//
//TEST_F(CacheSkipListTest, EraseTest) {
//  Key key1 = 10;
//  Key key2 = 20;
//  Key key3 = 30;
//  Arena arena;
//  TestComparator cmp;
//  CacheSkipList<TestComparator> list_(cmp, &arena);
//
//  // 插入键值对
//  Insert(&list_, key1);
//  Insert(&list_, key2);
//  Insert(&list_, key3);
//
//  // 验证键值对存在
//  VerifyKeyExists(&list_, key1);
//  VerifyKeyExists(&list_, key2);
//  VerifyKeyExists(&list_, key3);
//
//  // 删除键值对
//  ASSERT_TRUE(list_.Erase(Encode(&key2)));
//
//  // 验证键值对不存在
//  VerifyKeyNotExists(&list_, key2);
//
//  // 验证其他键值对仍然存在
//  VerifyKeyExists(&list_, key1);
//  VerifyKeyExists(&list_, key3);
//}
//
//TEST_F(CacheSkipListTest, Empty) {
//  Arena arena;
//  TestComparator cmp;
//  CacheSkipList<TestComparator> list(cmp, &arena);
//  Key key = 10;
//  ASSERT_TRUE(!list.Contains(Encode(&key)));
//
//  CacheSkipList<TestComparator>::Iterator iter(&list);
//  ASSERT_TRUE(!iter.Valid());
//  iter.SeekToFirst();
//  ASSERT_TRUE(!iter.Valid());
//  key = 100;
//  iter.Seek(Encode(&key));
//  ASSERT_TRUE(!iter.Valid());
//  iter.SeekForPrev(Encode(&key));
//  ASSERT_TRUE(!iter.Valid());
//  iter.SeekToLast();
//  ASSERT_TRUE(!iter.Valid());
//}
//
//TEST_F(CacheSkipListTest, InsertAndLookup) {
//  const int N = 2000;
//  const int R = 5000;
//  Random rnd(1000);
//  std::set<Key> keys;
//  ConcurrentArena arena;
//  TestComparator cmp;
//  CacheSkipList<TestComparator> list(cmp, &arena);
//  for (int i = 0; i < N; i++) {
//    Key key = rnd.Next() % R;
//    if (keys.insert(key).second) {
//      char* buf = list.AllocateKey(sizeof(Key));
//      memcpy(buf, &key, sizeof(Key));
//      list.Insert(buf);
//    }
//  }
//
//  for (Key i = 0; i < R; i++) {
//    if (list.Contains(Encode(&i))) {
//      ASSERT_EQ(keys.count(i), 1U);
//    } else {
//      ASSERT_EQ(keys.count(i), 0U);
//    }
//  }
//
//  // Simple iterator tests
//  {
//    CacheSkipList<TestComparator>::Iterator iter(&list);
//    ASSERT_TRUE(!iter.Valid());
//
//    uint64_t zero = 0;
//    iter.Seek(Encode(&zero));
//    ASSERT_TRUE(iter.Valid());
//    ASSERT_EQ(*(keys.begin()), Decode(iter.key()));
//
//    uint64_t max_key = R - 1;
//    iter.SeekForPrev(Encode(&max_key));
//    ASSERT_TRUE(iter.Valid());
//    ASSERT_EQ(*(keys.rbegin()), Decode(iter.key()));
//
//    iter.SeekToFirst();
//    ASSERT_TRUE(iter.Valid());
//    ASSERT_EQ(*(keys.begin()), Decode(iter.key()));
//
//    iter.SeekToLast();
//    ASSERT_TRUE(iter.Valid());
//    ASSERT_EQ(*(keys.rbegin()), Decode(iter.key()));
//  }
//
//  // Forward iteration test
//  for (Key i = 0; i < R; i++) {
//    CacheSkipList<TestComparator>::Iterator iter(&list);
//    iter.Seek(Encode(&i));
//
//    // Compare against model iterator
//    std::set<Key>::iterator model_iter = keys.lower_bound(i);
//    for (int j = 0; j < 3; j++) {
//      if (model_iter == keys.end()) {
//        ASSERT_TRUE(!iter.Valid());
//        break;
//      } else {
//        ASSERT_TRUE(iter.Valid());
//        ASSERT_EQ(*model_iter, Decode(iter.key()));
//        ++model_iter;
//        iter.Next();
//      }
//    }
//  }
//
//  // Backward iteration test
//  for (Key i = 0; i < R; i++) {
//    CacheSkipList<TestComparator>::Iterator iter(&list);
//    iter.SeekForPrev(Encode(&i));
//
//    // Compare against model iterator
//    std::set<Key>::iterator model_iter = keys.upper_bound(i);
//    for (int j = 0; j < 3; j++) {
//      if (model_iter == keys.begin()) {
//        ASSERT_TRUE(!iter.Valid());
//        break;
//      } else {
//        ASSERT_TRUE(iter.Valid());
//        ASSERT_EQ(*--model_iter, Decode(iter.key()));
//        iter.Prev();
//      }
//    }
//  }
//}
//
//TEST_F(CacheSkipListTest, InsertWithHint_Sequential) {
//  const int N = 100000;
//  Arena arena;
//  TestComparator cmp;
//  TestCacheSkipList list(cmp, &arena);
//  void* hint = nullptr;
//  for (int i = 0; i < N; i++) {
//    Key key = i;
//    InsertWithHint(&list, key, &hint);
//  }
//  Validate(&list);
//}
//
//TEST_F(CacheSkipListTest, InsertWithHint_MultipleHints) {
//  const int N = 100000;
//  const int S = 100;
//  Random rnd(534);
//  Arena arena;
//  TestComparator cmp;
//  TestCacheSkipList list(cmp, &arena);
//  void* hints[S];
//  Key last_key[S];
//  for (int i = 0; i < S; i++) {
//    hints[i] = nullptr;
//    last_key[i] = 0;
//  }
//  for (int i = 0; i < N; i++) {
//    Key s = rnd.Uniform(S);
//    Key key = (s << 32) + (++last_key[s]);
//    InsertWithHint(&list, key, &hints[s]);
//  }
//  Validate(&list);
//}
//
//TEST_F(CacheSkipListTest, InsertWithHint_MultipleHintsRandom) {
//  const int N = 100000;
//  const int S = 100;
//  Random rnd(534);
//  Arena arena;
//  TestComparator cmp;
//  TestCacheSkipList list(cmp, &arena);
//  void* hints[S];
//  for (int i = 0; i < S; i++) {
//    hints[i] = nullptr;
//  }
//  for (int i = 0; i < N; i++) {
//    Key s = rnd.Uniform(S);
//    Key key = (s << 32) + rnd.Next();
//    InsertWithHint(&list, key, &hints[s]);
//  }
//  Validate(&list);
//}
//
//TEST_F(CacheSkipListTest, InsertWithHint_CompatibleWithInsertWithoutHint) {
//  const int N = 100000;
//  const int S1 = 100;
//  const int S2 = 100;
//  Random rnd(534);
//  Arena arena;
//  TestComparator cmp;
//  TestCacheSkipList list(cmp, &arena);
//  std::unordered_set<Key> used;
//  Key with_hint[S1];
//  Key without_hint[S2];
//  void* hints[S1];
//  for (int i = 0; i < S1; i++) {
//    hints[i] = nullptr;
//    while (true) {
//      Key s = rnd.Next();
//      if (used.insert(s).second) {
//        with_hint[i] = s;
//        break;
//      }
//    }
//  }
//  for (int i = 0; i < S2; i++) {
//    while (true) {
//      Key s = rnd.Next();
//      if (used.insert(s).second) {
//        without_hint[i] = s;
//        break;
//      }
//    }
//  }
//  for (int i = 0; i < N; i++) {
//    Key s = rnd.Uniform(S1 + S2);
//    if (s < S1) {
//      Key key = (with_hint[s] << 32) + rnd.Next();
//      InsertWithHint(&list, key, &hints[s]);
//    } else {
//      Key key = (without_hint[s - S1] << 32) + rnd.Next();
//      Insert(&list, key);
//    }
//  }
//  Validate(&list);
//}
//
//#if !defined(ROCKSDB_VALGRIND_RUN) || defined(ROCKSDB_FULL_VALGRIND_RUN)
//// We want to make sure that with a single writer and multiple
//// concurrent readers (with no synchronization other than when a
//// reader's iterator is created), the reader always observes all the
//// data that was present in the skip list when the iterator was
//// constructor.  Because insertions are happening concurrently, we may
//// also observe new values that were inserted since the iterator was
//// constructed, but we should never miss any values that were present
//// at iterator construction time.
////
//// We generate multi-part keys:
////     <key,gen,hash>
//// where:
////     key is in range [0..K-1]
////     gen is a generation number for key
////     hash is hash(key,gen)
////
//// The insertion code picks a random key, sets gen to be 1 + the last
//// generation number inserted for that key, and sets hash to Hash(key,gen).
////
//// At the beginning of a read, we snapshot the last inserted
//// generation number for each key.  We then iterate, including random
//// calls to Next() and Seek().  For every key we encounter, we
//// check that it is either expected given the initial snapshot or has
//// been concurrently added since the iterator started.
//class ConcurrentTest {
// public:
//  static const uint32_t K = 8;
//
// private:
//  static uint64_t key(Key key) { return (key >> 40); }
//  static uint64_t gen(Key key) { return (key >> 8) & 0xffffffffu; }
//  static uint64_t hash(Key key) { return key & 0xff; }
//
//  static uint64_t HashNumbers(uint64_t k, uint64_t g) {
//    uint64_t data[2] = {k, g};
//    return Hash(reinterpret_cast<char*>(data), sizeof(data), 0);
//  }
//
//  static Key MakeKey(uint64_t k, uint64_t g) {
//    assert(sizeof(Key) == sizeof(uint64_t));
//    assert(k <= K);  // We sometimes pass K to seek to the end of the skiplist
//    assert(g <= 0xffffffffu);
//    return ((k << 40) | (g << 8) | (HashNumbers(k, g) & 0xff));
//  }
//
//  static bool IsValidKey(Key k) {
//    return hash(k) == (HashNumbers(key(k), gen(k)) & 0xff);
//  }
//
//  static Key RandomTarget(Random* rnd) {
//    switch (rnd->Next() % 10) {
//      case 0:
//        // Seek to beginning
//        return MakeKey(0, 0);
//      case 1:
//        // Seek to end
//        return MakeKey(K, 0);
//      default:
//        // Seek to middle
//        return MakeKey(rnd->Next() % K, 0);
//    }
//  }
//
//  // Per-key generation
//  struct State {
//    std::atomic<int> generation[K];
//    void Set(int k, int v) {
//      generation[k].store(v, std::memory_order_release);
//    }
//    int Get(int k) { return generation[k].load(std::memory_order_acquire); }
//
//    State() {
//      for (unsigned int k = 0; k < K; k++) {
//        Set(k, 0);
//      }
//    }
//  };
//
//  // Current state of the test
//  State current_;
//
//  ConcurrentArena arena_;
//
//  // CacheSkipList is not protected by mu_.  We just use a single writer
//  // thread to modify it.
//  CacheSkipList<TestComparator> list_;
//
// public:
//  ConcurrentTest() : list_(TestComparator(), &arena_) {}
//
//  // REQUIRES: No concurrent calls to WriteStep or ConcurrentWriteStep
//  void WriteStep(Random* rnd) {
//    const uint32_t k = rnd->Next() % K;
//    const int g = current_.Get(k) + 1;
//    const Key new_key = MakeKey(k, g);
//    char* buf = list_.AllocateKey(sizeof(Key));
//    memcpy(buf, &new_key, sizeof(Key));
//    list_.Insert(buf);
//    current_.Set(k, g);
//  }
//
//  // REQUIRES: No concurrent calls for the same k
//  void ConcurrentWriteStep(uint32_t k, bool use_hint = false) {
//    const int g = current_.Get(k) + 1;
//    const Key new_key = MakeKey(k, g);
//    char* buf = list_.AllocateKey(sizeof(Key));
//    memcpy(buf, &new_key, sizeof(Key));
//    if (use_hint) {
//      void* hint = nullptr;
//      list_.InsertWithHintConcurrently(buf, &hint);
//      delete[] reinterpret_cast<char*>(hint);
//    } else {
//      list_.InsertConcurrently(buf);
//    }
//    ASSERT_EQ(g, current_.Get(k) + 1);
//    current_.Set(k, g);
//  }
//
//  void ReadStep(Random* rnd) {
//    // Remember the initial committed state of the skiplist.
//    State initial_state;
//    for (unsigned int k = 0; k < K; k++) {
//      initial_state.Set(k, current_.Get(k));
//    }
//
//    Key pos = RandomTarget(rnd);
//    CacheSkipList<TestComparator>::Iterator iter(&list_);
//    iter.Seek(Encode(&pos));
//    while (true) {
//      Key current;
//      if (!iter.Valid()) {
//        current = MakeKey(K, 0);
//      } else {
//        current = Decode(iter.key());
//        ASSERT_TRUE(IsValidKey(current)) << current;
//      }
//      ASSERT_LE(pos, current) << "should not go backwards";
//
//      // Verify that everything in [pos,current) was not present in
//      // initial_state.
//      while (pos < current) {
//        ASSERT_LT(key(pos), K) << pos;
//
//        // Note that generation 0 is never inserted, so it is ok if
//        // <*,0,*> is missing.
//        ASSERT_TRUE((gen(pos) == 0U) ||
//                    (gen(pos) > static_cast<uint64_t>(initial_state.Get(
//                                    static_cast<int>(key(pos))))))
//            << "key: " << key(pos) << "; gen: " << gen(pos)
//            << "; initgen: " << initial_state.Get(static_cast<int>(key(pos)));
//
//        // Advance to next key in the valid key space
//        if (key(pos) < key(current)) {
//          pos = MakeKey(key(pos) + 1, 0);
//        } else {
//          pos = MakeKey(key(pos), gen(pos) + 1);
//        }
//      }
//
//      if (!iter.Valid()) {
//        break;
//      }
//
//      if (rnd->Next() % 2) {
//        iter.Next();
//        pos = MakeKey(key(pos), gen(pos) + 1);
//      } else {
//        Key new_target = RandomTarget(rnd);
//        if (new_target > pos) {
//          pos = new_target;
//          iter.Seek(Encode(&new_target));
//        }
//      }
//    }
//  }
//};
//const uint32_t ConcurrentTest::K;
//
//// Simple test that does single-threaded testing of the ConcurrentTest
//// scaffolding.
//TEST_F(CacheSkipListTest, ConcurrentReadWithoutThreads) {
//  ConcurrentTest test;
//  Random rnd(test::RandomSeed());
//  for (int i = 0; i < 10000; i++) {
//    test.ReadStep(&rnd);
//    test.WriteStep(&rnd);
//  }
//}
//
//TEST_F(CacheSkipListTest, ConcurrentInsertWithoutThreads) {
//  ConcurrentTest test;
//  Random rnd(test::RandomSeed());
//  for (int i = 0; i < 10000; i++) {
//    test.ReadStep(&rnd);
//    uint32_t base = rnd.Next();
//    for (int j = 0; j < 4; ++j) {
//      test.ConcurrentWriteStep((base + j) % ConcurrentTest::K);
//    }
//  }
//}
//
//class TestState {
// public:
//  ConcurrentTest t_;
//  bool use_hint_;
//  int seed_;
//  std::atomic<bool> quit_flag_;
//  std::atomic<uint32_t> next_writer_;
//
//  enum ReaderState { STARTING, RUNNING, DONE };
//
//  explicit TestState(int s)
//      : seed_(s),
//        quit_flag_(false),
//        state_(STARTING),
//        pending_writers_(0),
//        state_cv_(&mu_) {}
//
//  void Wait(ReaderState s) {
//    mu_.Lock();
//    while (state_ != s) {
//      state_cv_.Wait();
//    }
//    mu_.Unlock();
//  }
//
//  void Change(ReaderState s) {
//    mu_.Lock();
//    state_ = s;
//    state_cv_.Signal();
//    mu_.Unlock();
//  }
//
//  void AdjustPendingWriters(int delta) {
//    mu_.Lock();
//    pending_writers_ += delta;
//    if (pending_writers_ == 0) {
//      state_cv_.Signal();
//    }
//    mu_.Unlock();
//  }
//
//  void WaitForPendingWriters() {
//    mu_.Lock();
//    while (pending_writers_ != 0) {
//      state_cv_.Wait();
//    }
//    mu_.Unlock();
//  }
//
// private:
//  port::Mutex mu_;
//  ReaderState state_;
//  int pending_writers_;
//  port::CondVar state_cv_;
//};
//
//static void ConcurrentReader(void* arg) {
//  TestState* state = static_cast<TestState*>(arg);
//  Random rnd(state->seed_);
//  int64_t reads = 0;
//  state->Change(TestState::RUNNING);
//  while (!state->quit_flag_.load(std::memory_order_acquire)) {
//    state->t_.ReadStep(&rnd);
//    ++reads;
//  }
//  (void)reads;
//  state->Change(TestState::DONE);
//}
//
//static void ConcurrentWriter(void* arg) {
//  TestState* state = static_cast<TestState*>(arg);
//  uint32_t k = state->next_writer_++ % ConcurrentTest::K;
//  state->t_.ConcurrentWriteStep(k, state->use_hint_);
//  state->AdjustPendingWriters(-1);
//}
//
//static void RunConcurrentRead(int run) {
//  const int seed = test::RandomSeed() + (run * 100);
//  Random rnd(seed);
//  const int N = 1000;
//  const int kSize = 1000;
//  for (int i = 0; i < N; i++) {
//    if ((i % 100) == 0) {
//      fprintf(stderr, "Run %d of %d\n", i, N);
//    }
//    TestState state(seed + 1);
//    Env::Default()->SetBackgroundThreads(1);
//    Env::Default()->Schedule(ConcurrentReader, &state);
//    state.Wait(TestState::RUNNING);
//    for (int k = 0; k < kSize; ++k) {
//      state.t_.WriteStep(&rnd);
//    }
//    state.quit_flag_.store(true, std::memory_order_release);
//    state.Wait(TestState::DONE);
//  }
//}
//
//static void RunConcurrentInsert(int run, bool use_hint = false,
//                                int write_parallelism = 4) {
//  Env::Default()->SetBackgroundThreads(1 + write_parallelism,
//                                       Env::Priority::LOW);
//  const int seed = test::RandomSeed() + (run * 100);
//  Random rnd(seed);
//  const int N = 1000;
//  const int kSize = 1000;
//  for (int i = 0; i < N; i++) {
//    if ((i % 100) == 0) {
//      fprintf(stderr, "Run %d of %d\n", i, N);
//    }
//    TestState state(seed + 1);
//    state.use_hint_ = use_hint;
//    Env::Default()->Schedule(ConcurrentReader, &state);
//    state.Wait(TestState::RUNNING);
//    for (int k = 0; k < kSize; k += write_parallelism) {
//      state.next_writer_ = rnd.Next();
//      state.AdjustPendingWriters(write_parallelism);
//      for (int p = 0; p < write_parallelism; ++p) {
//        Env::Default()->Schedule(ConcurrentWriter, &state);
//      }
//      state.WaitForPendingWriters();
//    }
//    state.quit_flag_.store(true, std::memory_order_release);
//    state.Wait(TestState::DONE);
//  }
//}
//
//TEST_F(CacheSkipListTest, ConcurrentRead1) { RunConcurrentRead(1); }
//TEST_F(CacheSkipListTest, ConcurrentRead2) { RunConcurrentRead(2); }
//TEST_F(CacheSkipListTest, ConcurrentRead3) { RunConcurrentRead(3); }
//TEST_F(CacheSkipListTest, ConcurrentRead4) { RunConcurrentRead(4); }
//TEST_F(CacheSkipListTest, ConcurrentRead5) { RunConcurrentRead(5); }
//TEST_F(CacheSkipListTest, ConcurrentInsert1) { RunConcurrentInsert(1); }
//TEST_F(CacheSkipListTest, ConcurrentInsert2) { RunConcurrentInsert(2); }
//TEST_F(CacheSkipListTest, ConcurrentInsert3) { RunConcurrentInsert(3); }
//TEST_F(CacheSkipListTest, ConcurrentInsertWithHint1) {
//  RunConcurrentInsert(1, true);
//}
//TEST_F(CacheSkipListTest, ConcurrentInsertWithHint2) {
//  RunConcurrentInsert(2, true);
//}
//TEST_F(CacheSkipListTest, ConcurrentInsertWithHint3) {
//  RunConcurrentInsert(3, true);
//}
//
//#endif  // !defined(ROCKSDB_VALGRIND_RUN) || defined(ROCKSDB_FULL_VALGRIND_RUN)
//}  // namespace ROCKSDB_NAMESPACE
//
//int main(int argc, char** argv) {
//  ROCKSDB_NAMESPACE::port::InstallStackTraceHandler();
//  ::testing::InitGoogleTest(&argc, argv);
//  return RUN_ALL_TESTS();
//}
