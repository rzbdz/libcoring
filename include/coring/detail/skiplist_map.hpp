// A simple Skiplist for timer.
// skiplist is slow when you don't need a linked-list traversal
// that's a problem. one another big problem is that the next pointers in
// a node is complete separated to origin node, which reduce the locality compared
// to map one, when map have their all data embeded in a single node, less indirections.
// Modeled after the one in the redis.
// I think if multiset/multimap(since we do need replicate keys) would be
// easy to use if we cannot make skiplist_map beat them on the performance.
// Also, I think lock-free would be the only benefits if we cannot beat std::multiset
// in general single threaded inserting + removing use cases.
// A fast skiplist_map may need a contiguous memory layout.
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

template <typename KeyType, typename ValueType, typename Allocator, KeyType minKey, KeyType maxKey>
struct SkiplistBase {
  constexpr static const double SKIPLIST_P = 0.25;
  constexpr static const int MAX_LEVEL = 32;
  struct SkiplistNode;
  using SkiplistNodeAlloc = typename std::allocator_traits<Allocator>::template rebind_alloc<SkiplistNode>;
  using PointerAlloc = typename std::allocator_traits<Allocator>::template rebind_alloc<SkiplistNode *>;
  struct SkiplistNode {
    std::pair<const KeyType, ValueType> data_;
    std::vector<SkiplistNode *, PointerAlloc> level_next_;
    SkiplistNode(KeyType key, const PointerAlloc &alloc) : data_{key, ValueType{}}, level_next_{MAX_LEVEL, alloc} {
      static_assert(std::is_default_constructible_v<ValueType>);
    }
    SkiplistNode(std::pair<const KeyType, ValueType> &&data, int size, const PointerAlloc &alloc)
        : data_{std::move(data)}, level_next_{size, alloc} {}
    SkiplistNode(KeyType key, ValueType val, int size, const PointerAlloc &alloc)
        : data_{std::move(key), std::move(val)}, level_next_(size, alloc) {}
  };

  struct SkiplistIterator {
    SkiplistNode *cur{nullptr};
    SkiplistIterator &operator++() {
      cur = cur->level_next_[0];
      return *this;
    }
    // Make it can't apply to rvalues anymore.
    SkiplistIterator operator++(int) {
      auto tmp = cur;
      cur = cur->level_next_[0];
      return SkiplistIterator{tmp};
    }
    friend bool operator!=(const SkiplistIterator &lhs, const SkiplistIterator &rhs) { return lhs.cur != rhs.cur; }
    friend bool operator==(const SkiplistIterator &lhs, const SkiplistIterator &rhs) { return lhs.cur == rhs.cur; }
    std::pair<const KeyType, ValueType> &operator*() { return cur->data_; }
    std::pair<const KeyType, ValueType> *operator->() { return &cur->data_; }
    const std::pair<const KeyType, ValueType> &operator*() const { return cur->data_; }
    const std::pair<const KeyType, ValueType> *operator->() const { return &cur->data_; }
    const KeyType &key() const { return cur->data_.first; }
    ValueType &value() { return cur->data_.second; }
    const ValueType &value() const { return cur->data_.second; }
  };
  struct SkiplistData {
    SkiplistNode head_;
    SkiplistNode *tail_{nullptr};
    int max_level_{1};
    size_t length_{0};
    explicit SkiplistData(const SkiplistNodeAlloc &alloc) : head_(minKey, alloc) {}
  };
  struct SkiplistImpl : protected SkiplistNodeAlloc, protected SkiplistData {
    using SkiplistData::head_;
    using SkiplistData::length_;
    using SkiplistData::max_level_;
    using SkiplistData::tail_;
    using allocator_type = SkiplistNodeAlloc;
    using allocator_type::allocate;
    using allocator_type::deallocate;
    explicit SkiplistImpl(const allocator_type &alloc) : allocator_type{alloc}, SkiplistData(allocator_type{*this}) {
      void *place = allocate(1);
      auto max_dummy = new (place) SkiplistNode{maxKey, PointerAlloc{*this}};  // implicit convert (rebind<U>)
      for (int i = MAX_LEVEL - 1; i >= 0; --i) {
        head_.level_next_[i] = max_dummy;
      }
      SkiplistData::tail_ = max_dummy;
    }
    ~SkiplistImpl() { destroy(head_.level_next_[0]); }

    static int random_level() {
      static thread_local std::default_random_engine gen_{static_cast<unsigned long>(time(0))};  // NOLINT
      std::bernoulli_distribution dis_{SKIPLIST_P};
      int level = 1;
      while (level < MAX_LEVEL && dis_(gen_)) ++level;
      return level;
    }

    // when we want to remove all expired times,
    // we do need lower_bound
    std::array<SkiplistNode *, MAX_LEVEL> find_prevs(const KeyType &key) {
      // begin from the first node
      SkiplistNode *cur = &head_;
      std::array<SkiplistNode *, MAX_LEVEL> prevs{};
      for (int i = max_level_ - 1; i >= 0; i--) {
        while (key > cur->level_next_[i]->data_.first) cur = cur->level_next_[i];
        prevs[i] = cur;
      }
      return prevs;
    }

    std::array<SkiplistNode *, MAX_LEVEL> find_upper_prevs(const KeyType &key) {
      // begin from the first node
      SkiplistNode *cur = &head_;
      std::array<SkiplistNode *, MAX_LEVEL> prevs{};
      for (int i = max_level_ - 1; i >= 0; i--) {
        while (key >= cur->level_next_[i]->data_.first) cur = cur->level_next_[i];
        prevs[i] = cur;
      }
      return prevs;
    }

    void destroy(SkiplistNode *first) {
      while (first) {
        auto *next = first->level_next_[0];
        allocator_type::deallocate(first, 1);
        first = next;
      }
    }

    bool contains(const KeyType &target) {
      SkiplistNode *cur = &head_;
      for (int i = max_level_ - 1; i >= 0; i--) {
        while (target > cur->level_next_[i]->data_.first) cur = cur->level_next_[i];
      }
      return cur->level_next_[0] && cur->level_next_[0]->data_.first == target;
    }

    SkiplistIterator begin() { return {(&head_)->level_next_[0]}; }

    SkiplistIterator end() { return {tail_}; }

    SkiplistIterator find(const KeyType &target) {
      SkiplistNode *cur = &head_;
      for (int i = max_level_ - 1; i >= 0; i--) {
        while (target > cur->level_next_[i]->data_.first) cur = cur->level_next_[i];
      }
      if (cur->level_next_[0]->data.first == target) {
        return {cur->level_next_[0]};
      } else {
        return end();
      }
    }

    const std::pair<KeyType, ValueType> &peek_first() { return head_.level_next_[0].data_; }

    [[nodiscard]] size_t size() const { return length_; }

    auto add(std::pair<const KeyType, ValueType> &&data) {
      ++length_;
      auto prevs = find_prevs(data.first);
      int level = random_level();
      [[unlikely]] if (level > max_level_) {
        for (int i = max_level_; i < level; i++) prevs[i] = &head_;
        max_level_ = level;
      }
      SkiplistNode *place = allocator_type::allocate(1);
      auto *cur = new (place) SkiplistNode(std::move(data), level, PointerAlloc{*this});
      for (int i = level - 1; i >= 0; i--) {
        cur->level_next_[i] = prevs[i]->level_next_[i];
        prevs[i]->level_next_[i] = cur;
      }
      return SkiplistIterator{place};
    }

    template <typename Key, typename Val>
    auto emplace(Key &&key, Val &&val) {
      ++length_;
      auto prevs = find_prevs(key);
      int level = random_level();
      [[unlikely]] if (level > max_level_) {
        for (int i = max_level_; i < level; i++) prevs[i] = &head_;
        max_level_ = level;
      }
      SkiplistNode *place = allocator_type::allocate(1);
      auto *cur = new (place) SkiplistNode(std::forward<Key>(key), std::forward<Val>(val), level, PointerAlloc{*this});
      for (int i = level - 1; i >= 0; i--) {
        cur->level_next_[i] = prevs[i]->level_next_[i];
        prevs[i]->level_next_[i] = cur;
      }
      return SkiplistIterator{place};
    }

    bool erase_one(const KeyType &key) {
      auto prevs = find_prevs(key);
      if (!prevs[0]->level_next_[0] || prevs[0]->level_next_[0]->data_.first != key) return false;
      SkiplistNode *del = prevs[0]->level_next_[0];
      for (size_t i = 0; i < del->level_next_.size(); i++) {
        prevs[i]->level_next_[i] = del->level_next_[i];
      }
      allocator_type::deallocate(del, 1);
      while (max_level_ > 1 && head_.level_next_[max_level_ - 1] == tail_) max_level_--;
      --length_;
      return true;
    }

    std::vector<ValueType> pop_less_eq(const KeyType &key) {
      std::vector<ValueType> res;
      auto prevs = find_upper_prevs(key);
      if (prevs[0] == &head_) {
        return res;
      }
      auto start = head_.level_next_[0];
      // link head to the rest of the list
      for (int i = 0; i < max_level_; ++i) {
        head_.level_next_[i] = prevs[i]->level_next_[i];
        if (prevs[i] != &head_) prevs[i]->level_next_[i] = nullptr;
      }
      auto cur = start;
      for (; cur;) {
        auto *next = cur->level_next_[0];
        res.emplace_back(std::move(cur->data_.second));
        allocator_type::deallocate(cur, 1);
        cur = next;
      }
      while (max_level_ > 1 && head_.level_next_[max_level_ - 1] == tail_) max_level_--;
      length_ -= res.size();
      return res;
    }

    void do_less_eq_then_pop(const KeyType &key, std::function<void(ValueType &)> f) {
      auto prevs = find_upper_prevs(key);
      if (prevs[0] == &head_) {
        return;
      }
      auto start = head_.level_next_[0];
      // link head to the rest of the list
      for (int i = 0; i < max_level_; ++i) {
        head_.level_next_[i] = prevs[i]->level_next_[i];
        if (prevs[i] != &head_) prevs[i]->level_next_[i] = nullptr;
      }
      auto cur = start;
      int cnt = 0;
      for (; cur;) {
        cnt++;
        auto *next = cur->level_next_[0];
        f(cur->data_.second);  // safe to insert new entry at here.
        allocator_type::deallocate(cur, 1);
        cur = next;
      }
      while (max_level_ > 1 && head_.level_next_[max_level_ - 1] == tail_) max_level_--;
      length_ -= cnt;
    }

    void print() {
      std::vector<std::vector<char>> lines(max_level_);
      std::vector<SkiplistNode *> prevs(max_level_);
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
      SkiplistNode *cur = &head_;
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
          if (cur->data_.first > cur->level_next_[i]->data_.first) {
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
  };
  SkiplistBase(const Allocator &alloc) : impl_{SkiplistNodeAlloc(alloc)} {}
  SkiplistImpl impl_;
};

}  // namespace detail

namespace experimental {

template <typename KeyType, typename ValueType, typename Allocator = std::allocator<std::byte>,
          KeyType minKey = std::numeric_limits<KeyType>::min(), KeyType maxKey = std::numeric_limits<KeyType>::max()>
class skiplist_map {
 private:
  using base_type = detail::SkiplistBase<KeyType, ValueType, Allocator, minKey, maxKey>;

 public:
  using key_type = KeyType;
  using value_type = std::pair<const KeyType, ValueType>;
  using allocator_type = Allocator;
  using iterator = typename base_type::SkiplistIterator;

 public:
  static constexpr decltype(sizeof(typename base_type::SkiplistNode)) avg_node_size =
      sizeof(typename base_type::SkiplistNode) + sizeof(void *) * 4;

 public:
  explicit skiplist_map(const Allocator &alloc = Allocator()) : base_{alloc} {}

  auto begin() { return base_.impl_.begin(); }

  auto end() { return base_.impl_.end(); }

  bool empty() const { return base_.impl_.length_ == 0; }

  size_t size() const { return base_.impl_.length_; }

  auto find(const KeyType &target) { return base_.impl_.find(target); }

  auto insert(const value_type &value) { return base_.impl_.add(std::make_pair(value.first, value.second)); }

  auto insert(value_type &&value) { return base_.impl_.add(std::move(value)); }

  template <typename K, typename V>
  auto emplace(K &&key, V &&val) {
    return base_.impl_.emplace(std::forward<K>(key), std::forward<V>(val));
  }

  inline bool erase_one(const KeyType &key) { return base_.impl_.erase_one(key); }

  inline std::vector<ValueType> pop_less_eq(const KeyType &key) { return base_.impl_.pop_less_eq(key); }

  /// NOTICE: f is free to insert new timer, but WARN you not to call erase_one inside f.
  inline void do_less_eq_then_pop(const KeyType &key, std::function<void(ValueType &)> f) {
    return base_.impl_.do_less_eq_then_pop(key, f);
  }

  inline void print() { return base_.impl_.print(); }

  inline auto correct() { return base_.impl_.check_correctness(); }

 private:
  base_type base_;
};

}  // namespace experimental
namespace pmr {
template <typename K, typename V, K minKey = std::numeric_limits<K>::min(), K maxKey = std::numeric_limits<K>::max()>
using skiplist_map = experimental::skiplist_map<K, V, std::pmr::polymorphic_allocator<std::byte>, minKey, maxKey>;
}
}  // namespace coring
#endif  // CORING_SKIPLIST_HPP
