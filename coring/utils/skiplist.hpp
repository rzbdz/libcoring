// Modeled after the one in the redis.
// I think if multiset/multimap(since we do need replicate keys) would be
// easy to use if we cannot make skiplist beat them on the performance.
// Also, I think lock-free would be the only benefits if we cannot beat std::multiset
// in general single threaded inserting + removing.
#include <climits>
#include <random>
#include <vector>

#ifndef CORING_SKIPLIST_HPP
#define CORING_SKIPLIST_HPP

namespace coring {
class skiplist {
 private:
  // possibility
  static const int SKIPLIST_P_VAL = RAND_MAX / 2, MAX_LEVEL = 16;
  // nodes arranged like this:
  // 0-1[] ------ -3[]---------- hi
  // 0-1[] --2[]---3[]-------5[]
  // 0-1[]---2[]---3[]--4[]--5[] lo
  // a vertical line would be a node, all val is stored only once
  struct _skiplist_node {
    int val_;
    std::vector<_skiplist_node *> level_next_;
    // TODO: it waste a lot of memory... auto shrinking is needed.
    explicit _skiplist_node(int val, int size = MAX_LEVEL) : val_(val), level_next_(size) {}
  };
  // random engine have to be per instance.
  // TODO: find a good seed, but I think it's not a big problem.
  std::default_random_engine gen_{};
  std::bernoulli_distribution dis_{0.25};
  // the first node of the list, just a dummy
  _skiplist_node head_;
  // the max level_next_ among all node
  int max_level_ = 1;

  /// lower_bound means the first one that >= the target, but sometimes
  /// we do reach the end of the list, or we just found the first bigger one.
  /// one extra check is required.
  /// find for remove, because if we use it for a timer, we would
  /// always remove the first key, and then update the next of head_.
  /// \param key
  /// \return
  std::vector<_skiplist_node *> _find_lower_bound_prevs(int key) {
    // begin from the first node
    _skiplist_node *cur = &head_;
    std::vector<_skiplist_node *> prevs(MAX_LEVEL);
    // ---say find 4----------------------------------------
    // nodes arranged like this:
    // 1[begin] d----- -3[]------------------ hi
    // 1[     ] r-2[]-r-3[]d------------5[]--
    // 1[     ]---2[]---3[]r-4[target]--5[]-- lo
    // -------------------------------------------
    // through every level_next_, from top to bottom
    for (int i = max_level_ - 1; i >= 0; i--) {
      // through elements in the current level_next_ with smaller value
      // TODO: to reduce bound checking, we should add a INT dummy at the end of the list...
      while (cur->level_next_[i] && cur->level_next_[i]->val_ < key) cur = cur->level_next_[i];
      // we have to set it one by one, since the prev is not one node
      prevs[i] = cur;
    }
    // we need this because we have to update all next-pointer of the previous nodes of our victim
    return prevs;
  }

 public:
  skiplist() : head_(INT_MIN, MAX_LEVEL) {}

  // TODO: memory leak
  // delete all nodes.
  ~skiplist() = default;

  // same as _find_lower_bound_prevs
  bool has(int target) {
    _skiplist_node *prev = _find_lower_bound_prevs(target)[0];
    return prev->level_next_[0] && prev->level_next_[0]->val_ == target;
  }

  void add(int num) {
    // 1,2,3,4,5,5,7
    // insert 5, just insert before  first 5.
    // insert 6, just insert before first bigger than 6 (since no 6).
    // we just get the prevs of it, then insert after it's prevs.
    auto prevs = _find_lower_bound_prevs(num);
    int level = random_level();
    // update max_level_ and prevs
    // because the first node must have MAX_LEVEL levels,
    // and the prevs only have max_level levels.
    if (level > max_level_) {
      for (int i = max_level_; i < level; i++) prevs[i] = &head_;
      max_level_ = level;
    }
    auto *cur = new _skiplist_node(num, level);
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

  bool erase(int num) {
    // simple too, we just found the first equal one, and then, we just do linked-list deleting chores.
    auto prevs = _find_lower_bound_prevs(num);
    if (!prevs[0]->level_next_[0] || prevs[0]->level_next_[0]->val_ != num) return false;
    _skiplist_node *del = prevs[0]->level_next_[0];
    // from prev->cur->next to prev->next
    for (int i = 0; i < max_level_; i++)
      // TODO: Question, Is there a way to know how many level the node to be deleted have?
      // So that we can reduce if branch, I think we can just add a field...
      // But the trade-off have to be benchmarked.
      if (prevs[i]->level_next_[i] == del) prevs[i]->level_next_[i] = del->level_next_[i];
    delete del;
    // update max_level_.
    // TODO:  Question, Is there a way to know who have the max_level ? but more memory consumption
    // I think we can use a counter array to do that.
    while (max_level_ > 1 && !head_.level_next_[max_level_ - 1]) max_level_--;
    // if there is backward point, need to set cur.next.back to cur.back
    return true;
  }

  int random_level() {
    int level = 1;
    while (dis_(gen_) && level < MAX_LEVEL) ++level;
    return level;
  }
};
}  // namespace coring
#endif  // CORING_SKIPLIST_HPP
