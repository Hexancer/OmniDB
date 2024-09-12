#include <chrono>
#include <iostream>
#include <map>
#include <string>
#include <thread>
#include <unordered_set>

#include "gtest/gtest.h"
#include "memtable/follyskiplist.h"
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

class FollySkipListTest : public ::testing::Test {
 protected:
  void SetUp() override {
    skiplist = new FollySkipList(32, &cmp, 1ULL << 20);
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
      ASSERT_EQ(p->data().Key(), s);
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
          ASSERT_EQ(p->Key(), e);
        } else {
          auto e = std::to_string(i + j);
          ASSERT_FALSE(p->Valid());
          ASSERT_EQ(std::string(p->Key()), SENTINEL_STR);
        }
      }
    }
  }

  void CheckAllNodes(std::vector<std::string> expected) {
    expected.insert(expected.begin(), SENTINEL_STR);
    expected.insert(expected.begin(), "");
    expected.push_back(TAIL);

    const std::vector<std::string>& actual = skiplist->DumpAllNodes();
    if (actual != expected) {
      fprintf(stderr, "Expected: \n");
      for (const auto& s : expected) {
        fprintf(stderr, "%s, ", s.c_str());
      }
      fprintf(stderr, "\n");
      fprintf(stderr, "Actual: \n");
      for (const auto& s : actual) {
        fprintf(stderr, "%s, ", s.c_str());
      }
      fprintf(stderr, "\n");
      ASSERT_EQ(actual, expected);
    }
  }

  FollySkipList* skiplist;
  TestComparator cmp;
  int thread_count = 50;
  int base = 100;
};

TEST_F(FollySkipListTest, FindRandom) { srand(42); }

TEST_F(FollySkipListTest, Basic1) { CheckAllNodes({}); }
TEST_F(FollySkipListTest, Basic2) {
  CheckAllNodes({});
  InsertRange(10, 1);
  CheckAllNodes({"10", SENTINEL_STR});
}
TEST_F(FollySkipListTest, Basic3) {
  InsertRange(10, 2);
  CheckAllNodes({"10", "11", SENTINEL_STR});
}
TEST_F(FollySkipListTest, Basic4) {
  InsertRange(10, 2);
  InsertRange(15, 2);
  CheckAllNodes({"10", "11", SENTINEL_STR, "15", "16",
                 SENTINEL_STR});
}
//TEST_F(FollySkipListTest, BasicFind1) {
//  InsertRange(10, 2);
//  InsertRange(15, 2);
//
//  ASSERT_EQ(skiplist->Find("0")->data().IsSentinel(), true);
//  ASSERT_EQ(skiplist->Find("10")->data().IsSentinel(), true);
//  ASSERT_EQ(skiplist->Find("11")->data().Key(), "10");
//  ASSERT_EQ(skiplist->Find("12")->data().IsSentinel(), true);
//  ASSERT_EQ(skiplist->Find("13")->data().IsSentinel(), true);
//  ASSERT_EQ(skiplist->Find("14")->data().IsSentinel(), true);
//  ASSERT_EQ(skiplist->Find("15")->data().IsSentinel(), true);
//  ASSERT_EQ(skiplist->Find("16")->data().Key(), "15");
//  ASSERT_EQ(skiplist->Find("17")->data().IsSentinel(), true);
//}

TEST_F(FollySkipListTest, BasicSeek1) {
  InsertRange(10, 2);
  InsertRange(15, 2);

  FollySkipList::NodeType* pNode = skiplist->Seek("0");
  ASSERT_EQ(pNode->data().IsSentinel(), true);
  pNode = skiplist->Seek("10");
  ASSERT_EQ(pNode->data().Key(), "10");
  pNode = skiplist->Seek("11");
  ASSERT_EQ(pNode->data().Key(), "11");
  pNode = skiplist->Seek("12");
  ASSERT_EQ(pNode->data().IsSentinel(), true);
  pNode = skiplist->Seek("13");
  ASSERT_EQ(pNode->data().IsSentinel(), true);
  pNode = skiplist->Seek("14");
  ASSERT_EQ(pNode->data().IsSentinel(), true);
  pNode = skiplist->Seek("15");
  ASSERT_EQ(pNode->data().Key(), "15");
  pNode = skiplist->Seek("16");
  ASSERT_EQ(pNode->data().Key(), "16");
  pNode = skiplist->Seek("17");
  ASSERT_EQ(pNode->data().IsSentinel(), true);
}

TEST_F(FollySkipListTest, BasicOverlap) {
  InsertRange(10, 1);
  CheckAllNodes({"10", SENTINEL_STR});
  InsertRange(10, 1);
  CheckAllNodes({"10", SENTINEL_STR});
  InsertRange(10, 2);
  CheckAllNodes({"10", "11", SENTINEL_STR});
  InsertRange(15, 2);
  CheckAllNodes({"10", "11", SENTINEL_STR, "15", "16",
                 SENTINEL_STR});
}

TEST_F(FollySkipListTest, BasicOverlap1) {
  InsertRange(10, 2);
  CheckAllNodes({"10", "11", SENTINEL_STR});
  InsertRange(15, 2);
  CheckAllNodes({"10", "11", SENTINEL_STR, "15", "16",
                 SENTINEL_STR});
  AppendRange(12, 4);
  CheckAllNodes({"10", "11", "12", "13", "14", "15", "16",
                 SENTINEL_STR});
}

TEST_F(FollySkipListTest, BasicOverlap2) {
  InsertRange(10, 1);
  CheckAllNodes({"10", SENTINEL_STR});
  InsertRange(11, 1);
  CheckAllNodes(
      {"10", SENTINEL_STR, "11", SENTINEL_STR});
  AppendRange(101, 2);
  CheckAllNodes({"10", "101", "102", SENTINEL_STR, "11",
                 SENTINEL_STR});
  InsertRange(130, 1);
  CheckAllNodes({"10", "101", "102", SENTINEL_STR, "11",
                 SENTINEL_STR, "130", SENTINEL_STR});
}

TEST_F(FollySkipListTest, ConcurrentInsertTest) {
  std::vector<std::thread> threads;
  int single_insert_count = 10;

  for (int i = 0; i < thread_count; ++i) {
    int start = base + i * single_insert_count;
    threads.emplace_back(&FollySkipListTest::ConcurrentInsert, this, start,
                         single_insert_count);
  }

  for (auto& t : threads) {
    t.join();
  }

  // 检查插入的节点
  std::vector<std::string> expected;
  for (int i = base; i < base + thread_count * single_insert_count; ++i) {
    expected.push_back(std::to_string(i));
    expected.push_back(SENTINEL_STR);
  }

  if (0) std::cout << "=============================" << std::endl;
  //  skiplist->PrintAllNodes();
  CheckAllNodes(expected);
}

TEST_F(FollySkipListTest, ComprehensiveConcurrentInsertTest) {
  std::vector<std::thread> threads;
  int single_insert_count = 100;
  int overlap_count = 99;  // 每个线程插入区域的重叠部分

  // 启动多个线程并行插入
  for (int i = 0; i < thread_count; ++i) {
    int start = base + i * (single_insert_count - overlap_count);
    threads.emplace_back(&FollySkipListTest::ConcurrentInsert, this, start,
                         single_insert_count);
    // 0-100,50-150,100-200,150-250,200-300,250-350,300-400,350-450,400-500,450-550
  }

  // 等待所有线程完成
  for (auto& t : threads) {
    t.join();
  }

  // 检查插入的节点
  std::vector<std::string> expected;
  for (int i = base;
       i < base + thread_count * (single_insert_count - overlap_count) +
               overlap_count;
       ++i) {
    expected.push_back(std::to_string(i));
    expected.push_back(SENTINEL_STR);  // 末尾哨兵节点
  }

  //  skiplist->PrintAllNodes();
  CheckAllNodes(expected);
}

TEST_F(FollySkipListTest, MultiThreadedInsertRangeTest) {
  int single_insert_count = 10;
  std::vector<std::thread> threads;

  for (int i = 0; i < thread_count; ++i) {
    int start = base + i * single_insert_count;
    threads.emplace_back(&FollySkipListTest::ConcurrentInsertRange, this, start,
                         single_insert_count);
  }

  for (auto& t : threads) {
    t.join();
  }

  for (int i = base; i < base + thread_count * single_insert_count; ++i) {
    auto s = std::to_string(i);
    auto it = skiplist->Seek(s);
    ASSERT_NE(it, nullptr);
    ASSERT_EQ(it->data().Key(), s);
    ASSERT_EQ(it->data().Value(), s);
  }
}

TEST_F(FollySkipListTest, MultiThreadedOverlappingInsertRangeTest) {
  int single_insert_count = 15;
  int overlap_count = 10;  // 每个线程的插入范围重叠的部分
  std::vector<std::thread> threads;

  for (int i = 0; i < thread_count; ++i) {
    int start = base + i * (single_insert_count - overlap_count);  // 0-9 5-14
    threads.emplace_back(&FollySkipListTest::ConcurrentInsertRange, this, start,
                         single_insert_count);
  }

  for (auto& t : threads) {
    t.join();
  }

  std::vector<std::string> expected;
  for (int i = base;
       i < base + thread_count * (single_insert_count - overlap_count) +
               overlap_count;
       ++i) {
    expected.push_back(std::to_string(i));
    //    expected.push_back(SENTINEL_STR);  // 末尾哨兵节点
  }
  expected.push_back(SENTINEL_STR);  // 末尾哨兵节点

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

TEST_F(FollySkipListTest, BasicOverlapRandom) {
  using namespace std::chrono;
  auto ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch())
                .count();
  fprintf(stderr, "seed: %u\n", (uint32_t)ms);
  srand(uint32_t(ms));

  std::set<std::string> s{""};
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
//    auto act = skiplist->DumpAllNodesNoSentinel();
//    std::vector<std::string> ref(s.begin(), s.end());
//
//    skiplist->PrintAllNodes();
//    ASSERT_EQ(ref, act);
  }
}
}  // namespace ROCKSDB_NAMESPACE

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}