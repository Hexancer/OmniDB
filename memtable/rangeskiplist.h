//
// Created by geng on 6/17/24.
//

#pragma once

#include <cstdlib>  // for rand()
#include <ctime>    // for srand()
#include <iostream>
#include <memory>  // for smart pointers
#include <string>
#include <vector>

namespace rocksdb {

#define RANGESKIPLIST_MAXLEVEL 32  // Maximum levels
#define RANGESKIPLIST_P 0.25       // Probability factor

class RangeSkipListNode {
 public:
  //  typedef std::shared_ptr<RangeSkipListNode> NodePtrType;
  typedef RangeSkipListNode* NodePtrType;

  std::string left_;
  std::string right_;
  std::vector<NodePtrType> forward_;
  NodePtrType backward;

  RangeSkipListNode(const std::string& left, const std::string& right,
                    int level)
      : left_(left.data(), left.size()),
        right_(right.data(), right.size()),
        forward_(level, nullptr),
        backward(nullptr) {}
};

template <class Comparator>
class RangeSkipList {
 public:
  //  typedef std::shared_ptr<RangeSkipListNode> NodePtrType;
  typedef RangeSkipListNode* NodePtrType;

  explicit RangeSkipList(int maxLevel, const Comparator* cmp)
      : maxLevel_(maxLevel), currentLevel_(0), length_(0), cmp_(cmp) {
    head_ = new RangeSkipListNode("", "", maxLevel_);
    //  lastHit_ = nullptr;
    srand((unsigned)time(NULL));
  }
  void Insert(const Slice& left, const Slice& right) {
    std::vector<NodePtrType> update(maxLevel_);
    auto x = head_;

    // 找到插入位置
    for (int i = currentLevel_; i >= 0; i--) {
      while (x->forward_[i] &&
             cmpWrapper(Slice(x->forward_[i]->right_), left) < 0) {
        x = x->forward_[i];
      }
      update[i] = x;  // 记录前驱节点
    }
    x = x->forward_[0];

    // 初始化新的区间
    std::string new_left = left.ToString();
    std::string new_right = right.ToString();

    // 检查新的区间是否完全包含在已有区间中
    if (x && cmpWrapper(Slice(x->left_), left) <= 0 &&
        cmpWrapper(right, Slice(x->right_)) <= 0) {
      // 新区间完全包含在已有区间中，直接返回
      return;
    }

    // 合并重叠或相邻的区间
    while (x && cmpWrapper(Slice(x->left_), right) <= 0) {
      new_left = std::min(new_left, x->left_);
      new_right = std::max(new_right, x->right_);
      for (int i = 0; i <= currentLevel_; i++) {
        if (update[i]->forward_[i] == x) {
          update[i]->forward_[i] = x->forward_[i];
        }
      }
      x = x->forward_[0];
      length_--;
    }

    int newLevel = RandomLevel();
    if (newLevel > currentLevel_) {
      for (int i = currentLevel_ + 1; i < newLevel; i++) {
        update[i] = head_;
      }
      currentLevel_ = newLevel;
    }

    auto newNode = CreateNode(new_left, new_right, newLevel);
    for (int i = 0; i < newLevel; i++) {
      newNode->forward_[i] = update[i]->forward_[i];
      update[i]->forward_[i] = newNode;
    }
    length_++;
  }
  bool Delete(const Slice& left, const Slice& right) {
    std::vector<NodePtrType> update(maxLevel_);
    auto x = head_;
    for (int i = currentLevel_; i >= 0; i--) {
      while (x->forward_[i] &&
             (cmpWrapper(Slice(x->forward_[i]->left_), left) < 0 ||
              (cmpWrapper(Slice(x->forward_[i]->left_), left) == 0 &&
               cmpWrapper(Slice(x->forward_[i]->right_), right) < 0))) {
        x = x->forward_[i];
      }
      update[i] = x;
    }
    x = x->forward_[0];

    if (x && cmpWrapper(Slice(x->left_), left) == 0 &&
        cmpWrapper(Slice(x->right_), right) == 0) {  // convert string to Slice
      for (int i = 0; i <= currentLevel_; i++) {
        if (update[i]->forward_[i] != x) {
          break;
        }
        update[i]->forward_[i] = x->forward_[i];
      }
      while (currentLevel_ > 0 && head_->forward_[currentLevel_] == nullptr) {
        currentLevel_--;
      }
      length_--;
      return true;
    }
    return false;
  }
  bool Hit(const Slice& target) const {
    auto x = head_;
    for (int i = currentLevel_; i >= 0; i--) {
      while (x->forward_[i] &&
             cmpWrapper(Slice(x->forward_[i]->right_), target) < 0) {
        x = x->forward_[i];
      }
    }
    x = x->forward_[0];
    if (x && cmpWrapper(Slice(x->left_), target) <= 0 &&
        cmpWrapper(target, Slice(x->right_)) <= 0) {
      //    lastHit_ = x;
      return true;
    }
    return false;
  }
  bool Overstep(const Slice& target) const {
    // 先检查target是否落在某个区间里面
    auto x = head_;
    for (int i = currentLevel_; i >= 0; i--) {
      while (x->forward_[i] &&
             cmpWrapper(Slice(x->forward_[i]->right_), target) < 0) {
        x = x->forward_[i];
      }
    }
    x = x->forward_[0];
    if (x && cmpWrapper(Slice(x->left_), target) <= 0 &&
        cmpWrapper(target, Slice(x->right_)) <= 0) {
      // 检查target是否是区间的右边界
      if (cmpWrapper(target, Slice(x->right_)) == 0) {
        return true;
      }
      return false;
    }
    return true;
  }
  //  NodePtrType GetLastHit() const;

  class RangeSkipListIterator {
   public:
    explicit RangeSkipListIterator(RangeSkipList* skiplist)
        : skiplist_(skiplist), current_(skiplist->head_->forward_[0]) {}
    void Seek(const Slice& target) {
      current_ = skiplist_->head_;
      for (int i = skiplist_->currentLevel_; i >= 0; i--) {
        while (current_->forward_[i] &&
               skiplist_->cmpWrapper(Slice(current_->forward_[i]->right_),
                                     target) < 0) {
          current_ = current_->forward_[i];
        }
      }
      current_ = current_->forward_[0];
      if (!current_ ||
          skiplist_->cmpWrapper(Slice(current_->left_), target) > 0) {
        current_ = nullptr;
      }
    }
    void SeekToFirst() { current_ = skiplist_->head_->forward_[0]; }
    void SeekToLast() {
      current_ = skiplist_->head_;
      for (int i = skiplist_->currentLevel_; i >= 0; i--) {
        while (current_->forward_[i]) {
          current_ = current_->forward_[i];
        }
      }
    }
    void Next() {
      if (current_) {
        current_ = current_->forward_[0];
      }
    }
    bool Valid() const { return current_ != nullptr; }
    std::string key() const { return current_ ? current_->left_ : ""; }
    std::string rangeLeft() const { return current_ ? current_->left_ : ""; }
    std::string rangeRight() const { return current_ ? current_->right_ : ""; }

   private:
    RangeSkipList* skiplist_;
    NodePtrType current_;
  };

  std::unique_ptr<RangeSkipListIterator> NewIterator() {
    return std::make_unique<RangeSkipListIterator>(this);
  }

 private:
  int RandomLevel() {
    int level = 1;
    while ((rand() / double(RAND_MAX)) < RANGESKIPLIST_P &&
           level < RANGESKIPLIST_MAXLEVEL) {
      level++;
    }
    return level;
  }
  NodePtrType CreateNode(const std::string& left, const std::string& right,
                         int level) {
    return new RangeSkipListNode(left, right, level);
  }
  inline int cmpWrapper(const Slice& lhs, const Slice& rhs) const {
    return this->cmp_->Compare(lhs, rhs);
  }

  NodePtrType head_;
  int maxLevel_;
  int currentLevel_;
  unsigned long length_;
  const Comparator* cmp_;
};

}  // namespace rocksdb
