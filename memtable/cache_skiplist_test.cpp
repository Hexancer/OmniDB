#include <chrono>
#include <iostream>
#include <map>
#include <string>
#include <thread>
#include <unordered_set>

#include "gtest/gtest.h"
#include "memtable/cacheskiplist.h"
#include "rocksdb/slice.h"
#include "test_util/testharness.h"

namespace ROCKSDB_NAMESPACE {

using Key = Slice;

struct TestComparator: Comparator {
  int Compare(const Slice& a, const Slice& b) const override {
    return a.compare(b);
  }
  const char* Name() const override { return "TestComparator"; }
  void FindShortestSeparator(std::string* start,
                             const Slice& limit) const override {
    __builtin_unreachable();
  };
  void FindShortSuccessor(std::string* key) const override {
    __builtin_unreachable();
  };
};

class CacheSkipListTest : public ::testing::Test {
 protected:
  void SetUp() override {
    skiplist = new CacheSkipList(CACHESKIPLIST_MAXLEVEL, &cmp, 1ULL << 20);
  }

  void TearDown() override { delete skiplist; }

  void InsertRange(int x, int count) {
    assert(count > 0);
    for (int i = x; i < x + count; ++i) {
      auto s = std::to_string(i);
      if (i == x) {
        skiplist->Insert(s, s);
      } else {
        skiplist->Append(s, s);
      }
    }
  }
  void AppendRange(int x, int count) {
    assert(count > 0);
    for (int i = x; i < x + count; ++i) {
      auto s = std::to_string(i);
      skiplist->Append(s, s);
    }
  }

 public:
  void ConcurrentInsert(int start, int count) {
    for (int i = start; i < start + count; ++i) {
      auto s = std::to_string(i);
      skiplist->Insert(s, s);
    }
  }

  void ConcurrentInsertRange(int x, int count) {
    assert(count > 0);
    for (int i = x; i < x + count; ++i) {
      auto s = std::to_string(i);
      if (i == x) {
        skiplist->Insert(s, s);
      } else {
        skiplist->Append(s, s);
      }
    }
  }

 protected:
  void CheckSeekRange(int x, int count) {
    for (int i = x; i < x + count; ++i) {
      auto s = std::to_string(i);
      auto p = skiplist->Seek(s);
      ASSERT_EQ(p->Key(), s);
    }
  }

  void CheckNextRange(int x, int count) {
    for (int i = x; i < x + count; ++i) {
      auto s = std::to_string(i);
      for (int j = 0; i + j < x + count + 3; ++j) {
        auto p = skiplist->NewIterator();
        p->Seek(s);
        for (int k = 0; k < j; ++k) {
          if (p->Valid()) {
            p->Next();
          } else {
            break;
          }
        }
        if (i + j < x + count) {
          ASSERT_TRUE(p->Valid());
          auto e = std::to_string(i + j);
          ASSERT_EQ(p->key(), e);
        } else {
          auto e = std::to_string(i + j);
          ASSERT_FALSE(p->Valid());
          ASSERT_EQ(std::string(p->key().data_), SENTINEL_STR);
        }
      }
    }
  }

  void CheckAllNodes(std::vector<std::string> xs) {
    ASSERT_EQ(skiplist->DumpAllNodes(), xs);
  }

  CacheSkipList* skiplist;
  TestComparator cmp;
  int thread_count = 2;
  int base = 100;
};

TEST_F(CacheSkipListTest, FindRandom) { srand(42); }

TEST_F(CacheSkipListTest, Basic1) { CheckAllNodes({"", SENTINEL_STR, TAIL}); }
TEST_F(CacheSkipListTest, Basic2) {
  CheckAllNodes({"", SENTINEL_STR, TAIL});
  InsertRange(10, 1);
  CheckAllNodes({"", SENTINEL_STR, "10", SENTINEL_STR, TAIL});
}
TEST_F(CacheSkipListTest, Basic3) {
  InsertRange(10, 2);
  CheckAllNodes({"", SENTINEL_STR, "10", "11", SENTINEL_STR, TAIL});
}
TEST_F(CacheSkipListTest, Basic4) {
  InsertRange(10, 2);
  InsertRange(15, 2);
  CheckAllNodes({"", SENTINEL_STR, "10", "11", SENTINEL_STR, "15", "16",
                 SENTINEL_STR, TAIL});
}
TEST_F(CacheSkipListTest, BasicFind1) {
  InsertRange(10, 2);
  InsertRange(15, 2);

  ASSERT_EQ(skiplist->Find("0")->IsSentinel(), true);
  ASSERT_EQ(skiplist->Find("10")->IsSentinel(), true);
  ASSERT_EQ(skiplist->Find("11")->Key(), "10");
  ASSERT_EQ(skiplist->Find("12")->IsSentinel(), true);
  ASSERT_EQ(skiplist->Find("13")->IsSentinel(), true);
  ASSERT_EQ(skiplist->Find("14")->IsSentinel(), true);
  ASSERT_EQ(skiplist->Find("15")->IsSentinel(), true);
  ASSERT_EQ(skiplist->Find("16")->Key(), "15");
  ASSERT_EQ(skiplist->Find("17")->IsSentinel(), true);
}

TEST_F(CacheSkipListTest, BasicSeek1) {
  InsertRange(10, 2);
  InsertRange(15, 2);

  ASSERT_EQ(skiplist->Seek("0")->IsSentinel(), true);
  ASSERT_EQ(skiplist->Seek("10")->Key(), "10");
  ASSERT_EQ(skiplist->Seek("11")->Key(), "11");
  ASSERT_EQ(skiplist->Seek("12")->IsSentinel(), true);
  ASSERT_EQ(skiplist->Seek("13")->IsSentinel(), true);
  ASSERT_EQ(skiplist->Seek("14")->IsSentinel(), true);
  ASSERT_EQ(skiplist->Seek("15")->Key(), "15");
  ASSERT_EQ(skiplist->Seek("16")->Key(), "16");
  ASSERT_EQ(skiplist->Seek("17")->IsSentinel(), true);
}

TEST_F(CacheSkipListTest, BasicOverlap) {
  InsertRange(10, 1);
  CheckAllNodes({"", SENTINEL_STR, "10", SENTINEL_STR, TAIL});
  InsertRange(10, 1);
  CheckAllNodes({"", SENTINEL_STR, "10", SENTINEL_STR, TAIL});
  InsertRange(10, 2);
  CheckAllNodes({"", SENTINEL_STR, "10", "11", SENTINEL_STR, TAIL});
  InsertRange(15, 2);
  CheckAllNodes({"", SENTINEL_STR, "10", "11", SENTINEL_STR, "15", "16",
                 SENTINEL_STR, TAIL});
}

TEST_F(CacheSkipListTest, BasicOverlap1) {
  InsertRange(10, 2);
  CheckAllNodes({"", SENTINEL_STR, "10", "11", SENTINEL_STR, TAIL});
  InsertRange(15, 2);
  CheckAllNodes({"", SENTINEL_STR, "10", "11", SENTINEL_STR, "15", "16",
                 SENTINEL_STR, TAIL});
  AppendRange(12, 4);
  CheckAllNodes({"", SENTINEL_STR, "10", "11", "12", "13", "14", "15", "16",
                 SENTINEL_STR, TAIL});
}

TEST_F(CacheSkipListTest, BasicOverlap2) {
  InsertRange(10, 1);
  CheckAllNodes({"", SENTINEL_STR, "10", SENTINEL_STR, TAIL});
  InsertRange(11, 1);
  CheckAllNodes(
      {"", SENTINEL_STR, "10", SENTINEL_STR, "11", SENTINEL_STR, TAIL});
  AppendRange(101, 2);
  CheckAllNodes({"", SENTINEL_STR, "10", "101", "102", SENTINEL_STR, "11",
                 SENTINEL_STR, TAIL});
  InsertRange(130, 1);
  CheckAllNodes({"", SENTINEL_STR, "10", "101", "102", SENTINEL_STR, "11",
                 SENTINEL_STR, "130", SENTINEL_STR, TAIL});
}

TEST_F(CacheSkipListTest, ConcurrentInsertTest) {
  std::vector<std::thread> threads;
  int single_insert_count = 10;

  for (int i = 0; i < thread_count; ++i) {
    int start = base + i * single_insert_count;
    threads.emplace_back(&CacheSkipListTest::ConcurrentInsert, this, start,
                         single_insert_count);
  }

  for (auto& t : threads) {
    t.join();
  }

  // 检查插入的节点
  std::vector<std::string> expected;
  expected.push_back("");
  expected.push_back(SENTINEL_STR);
  for (int i = base; i < base + thread_count * single_insert_count; ++i) {
    expected.push_back(std::to_string(i));
    expected.push_back(SENTINEL_STR);
  }
  //  expected.push_back(SENTINEL_STR);  // 末尾哨兵节点
  expected.push_back(TAIL);  // 终止空节点

  if (0) std::cout << "=============================" << std::endl;
  //  skiplist->PrintAllNodes();
  CheckAllNodes(expected);
}

TEST_F(CacheSkipListTest, ComprehensiveConcurrentInsertTest) {
  std::vector<std::thread> threads;
  int single_insert_count = 100;
  int overlap_count = 99;  // 每个线程插入区域的重叠部分

  // 启动多个线程并行插入
  for (int i = 0; i < thread_count; ++i) {
    int start = base + i * (single_insert_count - overlap_count);
    threads.emplace_back(&CacheSkipListTest::ConcurrentInsert, this, start,
                         single_insert_count);
    // 0-100,50-150,100-200,150-250,200-300,250-350,300-400,350-450,400-500,450-550
  }

  // 等待所有线程完成
  for (auto& t : threads) {
    t.join();
  }

  // 检查插入的节点
  std::vector<std::string> expected;
  expected.push_back("");
  expected.push_back(SENTINEL_STR);
  for (int i = base;
       i < base + thread_count * (single_insert_count - overlap_count) +
               overlap_count;
       ++i) {
    expected.push_back(std::to_string(i));
    expected.push_back(SENTINEL_STR);  // 末尾哨兵节点
  }
  expected.push_back(TAIL);

  if (0) std::cout << "=============================" << std::endl;
  //  skiplist->PrintAllNodes();
  CheckAllNodes(expected);
}

TEST_F(CacheSkipListTest, MultiThreadedInsertRangeTest) {
  int single_insert_count = 10;
  std::vector<std::thread> threads;

  for (int i = 0; i < thread_count; ++i) {
    int start = base + i * single_insert_count;
    threads.emplace_back(&CacheSkipListTest::ConcurrentInsertRange, this, start,
                         single_insert_count);
  }

  for (auto& t : threads) {
    t.join();
  }

  for (int i = base; i < base + thread_count * single_insert_count; ++i) {
    auto s = std::to_string(i);
    auto it = skiplist->Find(s);
    ASSERT_NE(it, nullptr);
    ASSERT_EQ(it->Key(), s);
    ASSERT_EQ(it->Value(), s);
  }
}

TEST_F(CacheSkipListTest, MultiThreadedOverlappingInsertRangeTest) {
  int single_insert_count = 15;
  int overlap_count = 10;  // 每个线程的插入范围重叠的部分
  std::vector<std::thread> threads;

  for (int i = 0; i < thread_count; ++i) {
    int start = base + i * (single_insert_count - overlap_count);  // 0-9 5-14
    threads.emplace_back(&CacheSkipListTest::ConcurrentInsertRange, this, start,
                         single_insert_count);
  }

  for (auto& t : threads) {
    t.join();
  }

  std::vector<std::string> expected;
  expected.push_back("");            // 起始空节点
  expected.push_back(SENTINEL_STR);  // 末尾哨兵节点
  for (int i = base;
       i < base + thread_count * (single_insert_count - overlap_count) +
               overlap_count;
       ++i) {
    expected.push_back(std::to_string(i));
    //    expected.push_back(SENTINEL_STR);  // 末尾哨兵节点
  }
  expected.push_back(SENTINEL_STR);  // 末尾哨兵节点
  expected.push_back(TAIL);          // 终止空节点

  if (0) std::cout << "=============================" << std::endl;
  //  skiplist->PrintAllNodes();
  CheckAllNodes(expected);
  //  for (const auto& s : expected) {
  //    auto it = skiplist->Find(s);
  //    ASSERT_NE(it, nullptr);
  //    ASSERT_EQ(it->Key(), s);
  //    ASSERT_EQ(it->Value(), s);
  //  }
}

TEST_F(CacheSkipListTest, BasicOverlapRandom) {
  using namespace std::chrono;
  auto ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch())
                .count();
  fprintf(stderr, "seed: %u\n", (uint32_t)ms);
  srand(uint32_t(ms));

  std::set<std::string> s{"", TAIL};
  int MAX_ROUND = 1000;
  int KEY_LO = 100;
  int KEY_HI = 900;
  int KEY_LEN = 9;

  for (int round = 0; round < MAX_ROUND; ++round) {
    int start = KEY_LO + rand() % (KEY_HI - KEY_LO);
    for (int len = 0; len < rand() % KEY_LEN; ++len) {
      auto str = std::to_string(start + len);
      s.insert(str);
      if (len == 0) {
        skiplist->Insert(str, str);
      } else {
        skiplist->Append(str, str);
      }
    }
    auto act = skiplist->DumpAllNodesNoSentinel();
    std::vector<std::string> ref(s.begin(), s.end());

    skiplist->PrintAllNodes();
    ASSERT_EQ(ref, act);
  }
}
}  // namespace ROCKSDB_NAMESPACE

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}