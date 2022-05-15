// A simple Skiplist for timer.
// Modeled after the one in the redis.
// I think if multiset/multimap(since we do need replicate keys) would be
// easy to use if we cannot make skiplist_map beat them on the performance.
// Also, I think lock-free would be the only benefits if we cannot beat std::multiset
// in general single threaded inserting + removing use cases.
// A fast skiplist_map may need a contiguous memory layout.
// TODO: remove from iterator need a backward pointer(just like what redis did), or
//       there won't be any true 'lower_bound' function.
// TODO: a skiplist_map compatible to stl.
// TODO: There are some techniques introduced to implement a lock-free even wait-free.
// concurrent skip list.
// ref1: http://www.cl.cam.ac.uk/techreports/UCAM-CL-TR-579.pdf
// ref2: http://www.cs.tau.ac.il/~shanir/nir-pubs-web/Papers/OPODIS2006-BA.pdf
// Actually, we don't really need a thread-safe skiplist_map, since we can make a skiplist_map
// per thread (with coroutine & io_uring event loop).
#define UNSYNC
#include <climits>
#include <random>
#include <vector>
#include <mutex>
#include <iostream>
#include <memory_resource>
#include <list>
#include "debug.hpp"
#include "coring/detail/logging/fmt/format.h"

#ifndef CORING_SKIPLIST_HPP
#define CORING_SKIPLIST_HPP

namespace coring {
namespace detail {
template <typename KeyType>
struct default_skiplist_comparator {
  auto operator()(const KeyType &lessEq, const KeyType &moreEq) const { return lessEq <= moreEq; }
};
}  // namespace detail

/// Skiplist
/// \tparam KeyType
/// \tparam ValueType
/// \tparam Comparator Make sure your comparator is a function, and it should be
///                    like that: comparator(a, b) returns true if a <= b.
template <typename KeyType, typename ValueType, KeyType minKey, KeyType maxKey,
          typename Comparator = detail::default_skiplist_comparator<KeyType>>
class skiplist_map {
 private:
  // possibility
  constexpr static const double SKIPLIST_P = 0.25;
  constexpr static const int MAX_LEVEL = 16;
  // nodes arranged like this:
  // 0-1[] ------ -3[]---------- hi
  // 0-1[] --2[]---3[]-------5[]
  // 0-1[]---2[]---3[]--4[]--5[] lo
  // a vertical line would be a node, all val is stored only once
  struct _skiplist_node {
    std::pair<KeyType, ValueType> data_;
    // TODO: it waste a lot of memory... auto shrinking is needed.
    std::vector<_skiplist_node *> level_next_;
    _skiplist_node(KeyType key) : data_{key, ValueType{}}, level_next_(MAX_LEVEL) {
      static_assert(std::is_default_constructible_v<ValueType>);
    }
    explicit _skiplist_node(std::pair<KeyType, ValueType> &&data, int size = MAX_LEVEL)
        : data_{std::move(data)}, level_next_(size) {}
    explicit _skiplist_node(KeyType key, ValueType val, int size = MAX_LEVEL)
        : data_{std::move(key), std::move(val)}, level_next_(size) {}
  };
  Comparator IsLessEq{};
  size_t length_{0};

 public:
  void printKey() {
    std::vector<std::vector<char>> lines{max_level_};
    std::vector<_skiplist_node *> prevs{max_level_};
    for (int i = max_level_ - 1; i >= 0; --i) {
      auto o = std::back_inserter(lines[i]);
      fmt::vformat_to(o, "[{:2}]-->", fmt::make_format_args(head_.data_.first));
      prevs[i] = &head_;
    }
    for (auto a = &head_; a != tail_; a = a->level_next_[0]) {
      auto this_val = a->level_next_[0];
      for (int i = max_level_ - 1; i >= 0; --i) {
        if (prevs[i]->level_next_[i] == this_val) {
          auto o = std::back_inserter(lines[i]);
          fmt::vformat_to(o, "[{:2}]-->", fmt::make_format_args(this_val->data_.first));
          prevs[i] = this_val;
        } else {
          for (int j = 0; j < 7; j++) {
            lines[i].push_back('-');
          }
        }
      }
    }
    std::cout << "\n\n";
    for (int i = max_level_ - 1; i >= 0; --i) {
      for (auto c : lines[i]) {
        std::cout << c;
      }
      std::cout << std::endl;
    }
  }
  bool check_correctness() {
    bool is_out_of_order = false;
    _skiplist_node *cur = &head_;
    size_t count = 0;
    while (cur) {
      cur = cur->level_next_[0];
      count++;
      if (count >= 100000) {
        return false;
      }
    }
    if (count - 2 != size()) {
      return false;
    }
    count = 0;
    cur = &head_;
    for (int i = 0; i < MAX_LEVEL; i++) {
      while (cur->level_next_[i]) {
        if (!(IsLessEq(cur->data_.first, cur->level_next_[i]->data_.first))) {
          is_out_of_order = true;
          break;
        }
        cur = cur->level_next_[i];
        count++;
        if (count >= 100000) {
          return false;
        }
      }
      if (is_out_of_order) return false;
    }
    return true;
  }

 public:
  // remember this would be the prev of real node.
  struct skiplist_iterator {
    _skiplist_node *cur{nullptr};
    skiplist_iterator &operator++() {
      cur = cur->level_next_[0];
      return *this;
    }
    // Make it can't apply to rvalues anymore.
    const skiplist_iterator operator++(int) {
      auto tmp = cur;
      cur = cur->level_next_[0];
      return skiplist_iterator{tmp};
    }
    friend bool operator!=(skiplist_iterator lhs, skiplist_iterator rhs) { return lhs.cur != rhs.cur; }
    friend bool operator==(skiplist_iterator lhs, skiplist_iterator rhs) { return lhs.cur == rhs.cur; }
    const std::pair<KeyType, ValueType> &operator*() { return cur->level_next_[0]->data_; }
    const std::pair<KeyType, ValueType> *operator->() { return &cur->level_next_[0]->data_; }
    auto key() { return cur->level_next_[0]->data_.first; }
    auto value() { return cur->level_next_[0]->data_.second; }
  };

 private:
  // the first node of the list, just a dummy
  _skiplist_node head_;
  _skiplist_node *tail_{nullptr};
  // the max level_next_ among all node
  int max_level_{1};
  // very slow...
#ifndef UNSYNC
  std::mutex mutex_{};
#endif
 public:
  skiplist_map() : head_(minKey) {
    auto max_dummy = new _skiplist_node{maxKey};
    for (int i = MAX_LEVEL - 1; i >= 0; --i) {
      head_.level_next_[i] = max_dummy;
    }
    tail_ = max_dummy;
  }

  // TODO: check memory leak
  // delete all nodes.
  ~skiplist_map() { suicide_recursive(head_.level_next_[0]); }

 private:
  // when we want to remove all expired times,
  // we do need lower_bound
  template <bool IS_LOWER_BOUND, typename FKeyType>
  std::vector<_skiplist_node *> _find_prevs(FKeyType &&key) {
    // begin from the first node
    _skiplist_node *cur = &head_;
    std::vector<_skiplist_node *> prevs(MAX_LEVEL);
    // ---say find_bound 4----------------------------------------
    // nodes arranged like this:
    // 1[     ] ------------------------------hi-x
    // 1[begin] --2[]d---------------------------x
    // 1[     ] --2[]---3[]d------------5[]------x
    // 1[     ]---2[]---3[]r-4[target]--5[]-- lo-x
    // -------------------------------------------
    // notice that the 3 is prevs[0]
    //                 3 is prevs[1]
    //                 2 is prevs[2]
    // through every level_next_, from top to bottom
    for (int i = max_level_ - 1; i >= 0; i--) {
      // through elements in the current level_next_ with smaller value
      // TODO: to reduce bound checking, we should add a INT_MAX dummy at the end of the list...
      if constexpr (IS_LOWER_BOUND) {
        // while (cur->level_next_[i] && cur->level_next_[i]->val_ < key) cur = cur->level_next_[i];
        // cur->level_next_[i]->val_ < key, comparator returns a <= b.
        while (!(IsLessEq(key, cur->level_next_[i]->data_.first))) cur = cur->level_next_[i];
      } else {
        // while (cur->level_next_[i] && cur->level_next_[i]->val_ <= key) cur = cur->level_next_[i];
        // cur->level_next_[i]->val_ <= key
        while (IsLessEq(cur->level_next_[i]->data_.first, key)) cur = cur->level_next_[i];
      }
      // we have to set it one by one, since the prev is not one node
      prevs[i] = cur;
    }
    // we need this because we have to update all next-pointer of the previous nodes of our victim
    return prevs;
  }
  /// FIXME: this is a false lower_bound
  /// if there a not-unique key
  /// lower_bound means the first one that >= the target, but sometimes
  /// we do reach the end of the list, or we just found the first bigger one.
  /// one extra check is required.
  /// find_bound for remove, because if we use it for a timer, we would
  /// always remove the first key, and then update the next of head_.
  /// \param key
  /// \return
  template <typename FKeyType>
  std::vector<_skiplist_node *> _find_lower_bound_prevs(FKeyType &&key) {
    return _find_prevs<true>(std::forward<FKeyType>(key));
  }
  template <typename FKeyType>
  std::vector<_skiplist_node *> _find_upper_bound_prevs(FKeyType &&key) {
    return _find_prevs<false>(std::forward<FKeyType>(key));
  }

  static int random_level() {
    // random engine have to be per instance.
    // TODO: find_bound a good seed, but I think it's not a big problem.
    static thread_local std::default_random_engine gen_{static_cast<unsigned long>(time(0))};  // NOLINT
    std::bernoulli_distribution dis_{SKIPLIST_P};
    int level = 1;
    while (dis_(gen_) && level < MAX_LEVEL) ++level;
    return level;
  }

  void suicide_recursive(_skiplist_node *first) {
    if (first) {
      suicide_recursive(first->level_next_[0]);
      delete first;
    }
  }

 public:
  template <typename FKeyType>
  bool contains(FKeyType &&target) {
#ifndef UNSYNC
    std::lock_guard lk(mutex_);
#endif
    _skiplist_node *prev = _find_lower_bound_prevs(target)[0];
    return prev->level_next_[0] && prev->level_next_[0]->data_.first == target;
  }
  skiplist_iterator begin() { return {&head_}; }
  skiplist_iterator end() { return {tail_}; }

 private:
  template <bool IS_LOWER_BOUND = true, typename FKeyType>
  skiplist_iterator find_bound(FKeyType &&target) {
#ifndef UNSYNC
    std::lock_guard lk(mutex_);
#endif
    _skiplist_node *prev = _find_prevs<IS_LOWER_BOUND>(target)[0];
    if (prev->level_next_[0]->data.first == target) {
      return {prev->level_next_[0]};
    } else {
      return end();
    }
  }

 public:
  const std::pair<KeyType, ValueType> &peek_first() { return head_.level_next_[0].data_; }
  template <typename FKeyType>
  auto lower_bound(FKeyType &&tar) {
    return find_bound<true>(std::forward<FKeyType>(tar));
  }
  template <typename FKeyType>
  auto upper_bound(FKeyType &&tar) {
    return find_bound<false>(std::forward<FKeyType>(tar));
  }
  size_t size() const { return length_; }
  void add(std::pair<KeyType, ValueType> &&data) {
    // FIXME: We may want to leave the locking stuff to map user
    // if we only use this in single context a.k.a. single thread,
    // we don't want to use lock at all.
#ifndef UNSYNC
    std::lock_guard lk(mutex_);
#endif
    ++length_;
    // 1,2,3,4,5,5,7
    // insert 5, just insert before  first 5.
    // insert 6, just insert before first bigger than 6 (since no 6).
    // we just get the prevs of it, then insert after it's prevs.
    auto prevs = _find_lower_bound_prevs(data.first);
    int level = random_level();
    // update max_level_ and prevs
    // because the first node must have MAX_LEVEL levels,
    // and the prevs only have max_level levels.
    if (level > max_level_) {
      for (int i = max_level_; i < level; i++) prevs[i] = &head_;
      max_level_ = level;
    }
    auto *cur = new _skiplist_node(std::move(data), level);
    // linked-list inserting chores
    // from prev->next   to   prev->cur->next
    for (int i = level - 1; i >= 0; i--) {
      cur->level_next_[i] = prevs[i]->level_next_[i];
      prevs[i]->level_next_[i] = cur;
    }
    // if there is backward pointer, need to set both cur and cur.next 's back pointer
    // Note that the back point of the first valid node is nullptr instead of head_
    // we don't need a range query.
  }
  void insert(std::pair<KeyType, ValueType> &&data) { add(std::move(data)); }
  template <typename FKeyType, typename FValueType>
  void add(FKeyType &&key, FValueType &&val) {
    add(std::make_pair(std::forward<FKeyType>(key), std::forward<FValueType>(val)));
  }
  template <typename FKeyType, typename FValueType>
  void insert(FKeyType &&key, FValueType &&val) {
    add(std::make_pair(std::forward<FKeyType>(key), std::forward<FValueType>(val)));
  }

  template <typename FKeyType>
  bool erase_one(FKeyType &&key) {
#ifndef UNSYNC
    std::lock_guard lk(mutex_);
#endif
    // simple too, we just found the first equal one, and then, we just do linked-list deleting chores.
    // should we use std::forward ?
    auto prevs = _find_lower_bound_prevs(key);
    if (!prevs[0]->level_next_[0] || prevs[0]->level_next_[0]->data_.first != key) return false;
    _skiplist_node *del = prevs[0]->level_next_[0];
    // from prev->cur->next to prev->next
    for (size_t i = 0; i < del->level_next_.size(); i++) {
      // TODO: Is there a way to know how many level the node to be deleted have?
      // if (prevs[i]->level_next_[i] == del)
      prevs[i]->level_next_[i] = del->level_next_[i];
    }
    delete del;
    // update max_level_.
    // I think we can use a counter array to do that.
    while (max_level_ > 1 && head_.level_next_[max_level_ - 1] == tail_) max_level_--;
    // if there is backward point, need to set cur.next.back to cur.back
    --length_;
    return true;
  }

  template <typename FKeyType>
  std::vector<ValueType> pop_less_eq(FKeyType &&key) {
    std::vector<ValueType> res;
    // LOG_DEBUG_RAW("begin to pop check");
    // printKey();
    auto prevs = _find_upper_bound_prevs(std::forward<FKeyType>(key));
    // head......-> prevs[0] -> first bigger -> ... -> tail
    // if prev is not head, mean we got some nodes
    if (prevs[0] == &head_) {
      return res;
    }
    // now cut the prevs, make list like this:
    // head -> first bigger -> ... -> tail AND:
    // first -> ... -> last eq
    // ---say find_bound 4-----BEFORE------------------
    // nodes arranged like this:
    // 1[     ] --------------------------------hi-x
    // 1[begin] --2[x2]d---------------------------x
    // 1[     ] --2[x2]---3[x1]d------------5[]----x
    // 1[     ]---2[x2]---3[x0]r-4[target]--5[]-lo-x
    // --------------------------------------------
    // notice that the 3 is prevs[0]
    //                 3 is prevs[1]
    //                 2 is prevs[2]
    // ----------------AFTER--------------------
    // nodes arranged like this:
    // ---not in max_level----| 1[]--------hi-x
    // ---2[]d-prev2----------| 1[]-----------x
    // ---2[]---3[]d prev1----| 1[]--5[upper]------x
    // ---2[]---3[]r-4[ prev0]| 1[]--5[bound]-- lo-x
    // -------------------------------------------
    // this start could be the prevs, but must not the head_
    //    LOG_DEBUG_RAW("before cur");
    //    printKey();
    auto start = head_.level_next_[0];
    // link head to the rest of the list
    for (int i = 0; i < max_level_; ++i) {
      head_.level_next_[i] = prevs[i]->level_next_[i];
      if (prevs[i] != &head_) prevs[i]->level_next_[i] = nullptr;
    }
    //    LOG_DEBUG_RAW("after cur");
    //    printKey();

    // get results
    auto cur = start;
    for (; cur;) {
      res.emplace_back(std::move(cur->data_.second));
      cur = cur->level_next_[0];
    }
    // destroy
    suicide_recursive(start);
    while (max_level_ > 1 && head_.level_next_[max_level_ - 1] == tail_) max_level_--;
    length_ -= res.size();
    return res;
  }
};

namespace experiment {
/// Skiplist
/// \tparam KeyType
/// \tparam ValueType
/// \tparam Comparator Make sure your comparator is a function, and it should be
///                    like that: comparator(a, b) returns true if a <= b.
template <typename KeyType, typename ValueType, KeyType minKey, KeyType maxKey,
          typename Comparator = detail::default_skiplist_comparator<KeyType>>
class skiplist_map {
 private:
  struct _skiplist_node;

 public:
  typedef std::pmr::polymorphic_allocator<std::byte> Allocator;
  // remember this would be the prev of real node.
  typedef struct skiplist_iterator {
    _skiplist_node *cur{nullptr};
    skiplist_iterator &operator++() {
      cur = cur->level_next_[0];
      return *this;
    }
    // Make it can't apply to rvalues anymore.
    const skiplist_iterator operator++(int) {
      auto tmp = cur;
      cur = cur->level_next_[0];
      return skiplist_iterator{tmp};
    }
    friend bool operator!=(skiplist_iterator lhs, skiplist_iterator rhs) { return lhs.cur != rhs.cur; }
    friend bool operator==(skiplist_iterator lhs, skiplist_iterator rhs) { return lhs.cur == rhs.cur; }
    const std::pair<KeyType, ValueType> &operator*() { return cur->level_next_[0]->data_; }
    const std::pair<KeyType, ValueType> *operator->() { return &cur->level_next_[0]->data_; }
    auto key() { return cur->level_next_[0]->data_.first; }
    auto value() { return cur->level_next_[0]->data_.second; }
  } iterator;

 private:
  // possibility
  constexpr static const double SKIPLIST_P = 0.25;
  constexpr static const int MAX_LEVEL = 16;
  // nodes arranged like this:
  // 0-1[] ------ -3[]---------- hi
  // 0-1[] --2[]---3[]-------5[]
  // 0-1[]---2[]---3[]--4[]--5[] lo
  // a vertical line would be a node, all val is stored only once
  Allocator alloc_;
  struct _skiplist_node {
    std::pair<KeyType, ValueType> data_;
    std::pmr::vector<_skiplist_node *> level_next_;
    _skiplist_node(KeyType key, const Allocator &alloc) : data_{key, ValueType{}}, level_next_{MAX_LEVEL, alloc} {
      static_assert(std::is_default_constructible_v<ValueType>);
    }
    explicit _skiplist_node(std::pair<KeyType, ValueType> &&data, int size, const Allocator &alloc)
        : data_{std::move(data)}, level_next_{size, alloc} {}
    explicit _skiplist_node(KeyType key, ValueType val, int size, const Allocator &alloc)
        : data_{std::move(key), std::move(val)}, level_next_{size, alloc} {}
  };
  Comparator IsLessEq{};
  size_t length_{0};

 public:
  static constexpr decltype(sizeof(_skiplist_node)) node_size = sizeof(_skiplist_node) + sizeof(void *) * 4;
  void printKey() {
    std::vector<std::vector<char>> lines(max_level_);
    std::vector<_skiplist_node *> prevs(max_level_);
    for (int i = max_level_ - 1; i >= 0; --i) {
      auto o = std::back_inserter(lines[i]);
      fmt::vformat_to(o, "[{:2}]-->", fmt::make_format_args(head_.data_.first));
      prevs[i] = &head_;
    }
    for (auto a = &head_; a != tail_; a = a->level_next_[0]) {
      auto this_val = a->level_next_[0];
      for (int i = max_level_ - 1; i >= 0; --i) {
        if (prevs[i]->level_next_[i] == this_val) {
          auto o = std::back_inserter(lines[i]);
          fmt::vformat_to(o, "[{:2}]-->", fmt::make_format_args(this_val->data_.first));
          prevs[i] = this_val;
        } else {
          for (int j = 0; j < 7; j++) {
            lines[i].push_back('-');
          }
        }
      }
    }
    std::cout << "\n\n";
    for (int i = max_level_ - 1; i >= 0; --i) {
      for (auto c : lines[i]) {
        std::cout << c;
      }
      std::cout << std::endl;
    }
  }
  bool check_correctness() {
    bool is_out_of_order = false;
    _skiplist_node *cur = &head_;
    size_t count = 0;
    while (cur) {
      cur = cur->level_next_[0];
      count++;
      if (count >= 100000) {
        return false;
      }
    }
    if (count - 2 != size()) {
      return false;
    }
    count = 0;
    cur = &head_;
    for (int i = 0; i < MAX_LEVEL; i++) {
      while (cur->level_next_[i]) {
        if (!(IsLessEq(cur->data_.first, cur->level_next_[i]->data_.first))) {
          is_out_of_order = true;
          break;
        }
        cur = cur->level_next_[i];
        count++;
        if (count >= 100000) {
          return false;
        }
      }
      if (is_out_of_order) return false;
    }
    return true;
  }

 public:
 private:
  // the first node of the list, just a dummy
  _skiplist_node head_;
  _skiplist_node *tail_{nullptr};
  // the max level_next_ among all node
  int max_level_{1};
  // very slow...
#ifndef UNSYNC
  std::mutex mutex_{};
#endif

 public:
  explicit skiplist_map(const Allocator &alloc = std::pmr::new_delete_resource())
      : alloc_{alloc}, head_(minKey, alloc) {
    void *place = alloc_.allocate_bytes(sizeof(_skiplist_node), alignof(_skiplist_node));
    auto max_dummy = new (place) _skiplist_node{maxKey, alloc_};
    for (int i = MAX_LEVEL - 1; i >= 0; --i) {
      head_.level_next_[i] = max_dummy;
    }
    tail_ = max_dummy;
  }

  // TODO: check memory leak
  // delete all nodes.
  ~skiplist_map() { suicide_recursive(head_.level_next_[0]); }

 private:
  // when we want to remove all expired times,
  // we do need lower_bound
  template <bool IS_LOWER_BOUND, typename FKeyType>
  std::vector<_skiplist_node *> _find_prevs(FKeyType &&key) {
    // begin from the first node
    _skiplist_node *cur = &head_;
    std::vector<_skiplist_node *> prevs(MAX_LEVEL);
    // ---say find_bound 4----------------------------------------
    // nodes arranged like this:
    // 1[     ] ------------------------------hi-x
    // 1[begin] --2[]d---------------------------x
    // 1[     ] --2[]---3[]d------------5[]------x
    // 1[     ]---2[]---3[]r-4[target]--5[]-- lo-x
    // -------------------------------------------
    // notice that the 3 is prevs[0]
    //                 3 is prevs[1]
    //                 2 is prevs[2]
    // through every level_next_, from top to bottom
    for (int i = max_level_ - 1; i >= 0; i--) {
      // through elements in the current level_next_ with smaller value
      // TODO: to reduce bound checking, we should add a INT_MAX dummy at the end of the list...
      if constexpr (IS_LOWER_BOUND) {
        // while (cur->level_next_[i] && cur->level_next_[i]->val_ < key) cur = cur->level_next_[i];
        // cur->level_next_[i]->val_ < key, comparator returns a <= b.
        while (!(IsLessEq(key, cur->level_next_[i]->data_.first))) cur = cur->level_next_[i];
      } else {
        // while (cur->level_next_[i] && cur->level_next_[i]->val_ <= key) cur = cur->level_next_[i];
        // cur->level_next_[i]->val_ <= key
        while (IsLessEq(cur->level_next_[i]->data_.first, key)) cur = cur->level_next_[i];
      }
      // we have to set it one by one, since the prev is not one node
      prevs[i] = cur;
    }
    // we need this because we have to update all next-pointer of the previous nodes of our victim
    return prevs;
  }
  /// FIXME: this is a false lower_bound
  /// if there a not-unique key
  /// lower_bound means the first one that >= the target, but sometimes
  /// we do reach the end of the list, or we just found the first bigger one.
  /// one extra check is required.
  /// find_bound for remove, because if we use it for a timer, we would
  /// always remove the first key, and then update the next of head_.
  /// \param key
  /// \return
  template <typename FKeyType>
  std::vector<_skiplist_node *> _find_lower_bound_prevs(FKeyType &&key) {
    return _find_prevs<true>(std::forward<FKeyType>(key));
  }
  template <typename FKeyType>
  std::vector<_skiplist_node *> _find_upper_bound_prevs(FKeyType &&key) {
    return _find_prevs<false>(std::forward<FKeyType>(key));
  }

  static int random_level() {
    // random engine have to be per instance.
    // TODO: find_bound a good seed, but I think it's not a big problem.
    static thread_local std::default_random_engine gen_{static_cast<unsigned long>(time(0))};  // NOLINT
    std::bernoulli_distribution dis_{SKIPLIST_P};
    int level = 1;
    while (dis_(gen_) && level < MAX_LEVEL) ++level;
    return level;
  }

  void suicide_recursive(_skiplist_node *first) {
    if (first) {
      suicide_recursive(first->level_next_[0]);
      alloc_.deallocate_bytes(first, sizeof(*first), alignof(_skiplist_node));
    }
  }

 public:
  template <typename FKeyType>
  bool contains(FKeyType &&target) {
#ifndef UNSYNC
    std::lock_guard lk(mutex_);
#endif
    _skiplist_node *prev = _find_lower_bound_prevs(target)[0];
    return prev->level_next_[0] && prev->level_next_[0]->data_.first == target;
  }
  skiplist_iterator begin() { return {&head_}; }
  skiplist_iterator end() { return {tail_}; }

 private:
  template <bool IS_LOWER_BOUND = true, typename FKeyType>
  skiplist_iterator find_bound(FKeyType &&target) {
#ifndef UNSYNC
    std::lock_guard lk(mutex_);
#endif
    _skiplist_node *prev = _find_prevs<IS_LOWER_BOUND>(target)[0];
    if (prev->level_next_[0]->data.first == target) {
      return {prev->level_next_[0]};
    } else {
      return end();
    }
  }

 public:
  const std::pair<KeyType, ValueType> &peek_first() { return head_.level_next_[0].data_; }
  template <typename FKeyType>
  auto lower_bound(FKeyType &&tar) {
    return find_bound<true>(std::forward<FKeyType>(tar));
  }
  template <typename FKeyType>
  auto upper_bound(FKeyType &&tar) {
    return find_bound<false>(std::forward<FKeyType>(tar));
  }
  size_t size() const { return length_; }
  void add(std::pair<KeyType, ValueType> &&data) {
    // FIXME: We may want to leave the locking stuff to map user
    // if we only use this in single context a.k.a. single thread,
    // we don't want to use lock at all.
#ifndef UNSYNC
    std::lock_guard lk(mutex_);
#endif
    ++length_;
    // 1,2,3,4,5,5,7
    // insert 5, just insert before  first 5.
    // insert 6, just insert before first bigger than 6 (since no 6).
    // we just get the prevs of it, then insert after it's prevs.
    auto prevs = _find_lower_bound_prevs(data.first);
    int level = random_level();
    // update max_level_ and prevs
    // because the first node must have MAX_LEVEL levels,
    // and the prevs only have max_level levels.
    if (level > max_level_) {
      for (int i = max_level_; i < level; i++) prevs[i] = &head_;
      max_level_ = level;
    }
    void *place = alloc_.allocate_bytes(sizeof(_skiplist_node), alignof(_skiplist_node));
    auto *cur = new (place) _skiplist_node(std::move(data), level, alloc_);
    // linked-list inserting chores
    // from prev->next   to   prev->cur->next
    for (int i = level - 1; i >= 0; i--) {
      cur->level_next_[i] = prevs[i]->level_next_[i];
      prevs[i]->level_next_[i] = cur;
    }
    // if there is backward pointer, need to set both cur and cur.next 's back pointer
    // Note that the back point of the first valid node is nullptr instead of head_
    // we don't need a range query.
  }
  void insert(std::pair<KeyType, ValueType> &&data) { add(std::move(data)); }
  template <typename FKeyType, typename FValueType>
  void add(FKeyType &&key, FValueType &&val) {
    add(std::make_pair(std::forward<FKeyType>(key), std::forward<FValueType>(val)));
  }
  template <typename FKeyType, typename FValueType>
  void insert(FKeyType &&key, FValueType &&val) {
    add(std::make_pair(std::forward<FKeyType>(key), std::forward<FValueType>(val)));
  }

  template <typename FKeyType>
  bool erase_one(FKeyType &&key) {
#ifndef UNSYNC
    std::lock_guard lk(mutex_);
#endif
    // simple too, we just found the first equal one, and then, we just do linked-list deleting chores.
    // should we use std::forward ?
    auto prevs = _find_lower_bound_prevs(key);
    if (!prevs[0]->level_next_[0] || prevs[0]->level_next_[0]->data_.first != key) return false;
    _skiplist_node *del = prevs[0]->level_next_[0];
    // from prev->cur->next to prev->next
    for (size_t i = 0; i < del->level_next_.size(); i++) {
      // TODO: Is there a way to know how many level the node to be deleted have?
      // if (prevs[i]->level_next_[i] == del)
      prevs[i]->level_next_[i] = del->level_next_[i];
    }
    // delete del;
    alloc_.deallocate_bytes(del, sizeof(*del), alignof(_skiplist_node));
    // update max_level_.
    // I think we can use a counter array to do that.
    while (max_level_ > 1 && head_.level_next_[max_level_ - 1] == tail_) max_level_--;
    // if there is backward point, need to set cur.next.back to cur.back
    --length_;
    return true;
  }

  template <typename FKeyType>
  std::vector<ValueType> pop_less_eq(FKeyType &&key) {
    std::vector<ValueType> res;
    // LOG_DEBUG_RAW("begin to pop check");
    // printKey();
    auto prevs = _find_upper_bound_prevs(std::forward<FKeyType>(key));
    // head......-> prevs[0] -> first bigger -> ... -> tail
    // if prev is not head, mean we got some nodes
    if (prevs[0] == &head_) {
      return res;
    }
    // now cut the prevs, make list like this:
    // head -> first bigger -> ... -> tail AND:
    // first -> ... -> last eq
    // ---say find_bound 4-----BEFORE------------------
    // nodes arranged like this:
    // 1[     ] --------------------------------hi-x
    // 1[begin] --2[x2]d---------------------------x
    // 1[     ] --2[x2]---3[x1]d------------5[]----x
    // 1[     ]---2[x2]---3[x0]r-4[target]--5[]-lo-x
    // --------------------------------------------
    // notice that the 3 is prevs[0]
    //                 3 is prevs[1]
    //                 2 is prevs[2]
    // ----------------AFTER--------------------
    // nodes arranged like this:
    // ---not in max_level----| 1[]--------hi-x
    // ---2[]d-prev2----------| 1[]-----------x
    // ---2[]---3[]d prev1----| 1[]--5[upper]------x
    // ---2[]---3[]r-4[ prev0]| 1[]--5[bound]-- lo-x
    // -------------------------------------------
    // this start could be the prevs, but must not the head_
    //    LOG_DEBUG_RAW("before cur");
    //    printKey();
    auto start = head_.level_next_[0];
    // link head to the rest of the list
    for (int i = 0; i < max_level_; ++i) {
      head_.level_next_[i] = prevs[i]->level_next_[i];
      if (prevs[i] != &head_) prevs[i]->level_next_[i] = nullptr;
    }
    //    LOG_DEBUG_RAW("after cur");
    //    printKey();

    // get results
    auto cur = start;
    for (; cur;) {
      res.emplace_back(std::move(cur->data_.second));
      cur = cur->level_next_[0];
    }
    // destroy
    suicide_recursive(start);
    while (max_level_ > 1 && head_.level_next_[max_level_ - 1] == tail_) max_level_--;
    length_ -= res.size();
    return res;
  }
};
}  // namespace experiment
}  // namespace coring
#endif  // CORING_SKIPLIST_HPP
