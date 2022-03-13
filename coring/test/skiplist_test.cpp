// valgrind --tool=memcheck --leak-check=full --show-reachable=yes --trace-children=yes
// gtest --gtest_filter='XX.YY'
#include <gtest/gtest.h>
#include <coring/utils/skiplist_map.hpp>

TEST(SkipListTest, TestSimple) {
  coring::skiplist_map<int, int, INT_MIN, INT_MAX, 0> sk;
  for (int i = 0; i < 20; ++i) {
    sk.add(i, i);
  }
  // sk.printKey();
  EXPECT_EQ(sk.check_correctness(), true);
  EXPECT_EQ(sk.size(), 20);
  std::cout << std::endl;
  sk.erase_one(4);
  sk.erase_one(5);
  sk.erase_one(6);
  // test not exist!!
  sk.erase_one(99);
  //  sk.printKey();
  EXPECT_EQ(sk.size(), 20 - 3);
  EXPECT_EQ(sk.check_correctness(), true);
}

TEST(SkipListTest, TestAdd) {
  coring::skiplist_map<int, int, INT_MIN, INT_MAX, 0> sk;
  for (int i = 0; i < 10; ++i) {
    if (i == 1 || i == 5 || i == 7) {
      for (int j = 0; j < 3; ++j) {
        sk.add(i, i);
      }
    } else {
      sk.add(i, i);
    }
  }
  EXPECT_EQ(sk.size(), 16);
  sk.add(16, 1);
  sk.add(18, 1);
  sk.add(12, 1);
  sk.add(13, 1);
  EXPECT_EQ(sk.size(), 20);
  // sk.printKey();
  EXPECT_TRUE(sk.check_correctness());
}

TEST(SkipListTest, TestPopLessEqSimple1) {
  coring::skiplist_map<int, int, INT_MIN, INT_MAX, 0> sk;
  for (int i = 0; i < 10; ++i) {
    if (i == 1 || i == 5 || i == 7) {
      for (int j = 0; j < 3; ++j) {
        sk.add(i, i);
      }
    } else {
      sk.add(i, i + 2);
    }
  }
  sk.add(16, 1);
  sk.add(18, 1);
  sk.add(12, 1);
  sk.add(13, 1);
  //  LOG_DEBUG_RAW("print key 1");
  // sk.printKey();
  EXPECT_EQ(sk.size(), 20);
  EXPECT_TRUE(sk.check_correctness());
  auto res = sk.pop_less_eq(9);
  // sk.printKey();
  EXPECT_EQ(sk.size(), 4);
  EXPECT_EQ(res.size(), 16);
  int id = 0;
  for (int i = 0; i < 10; ++i) {
    if (i == 1 || i == 5 || i == 7) {
      for (int j = 0; j < 3; ++j) {
        EXPECT_EQ(i, res[id]);
        ++id;
      }
    } else {
      EXPECT_EQ(i + 2, res[id]);
      ++id;
    }
  }
  //  LOG_DEBUG_RAW("print key 2");
  // sk.printKey();
  EXPECT_TRUE(sk.check_correctness());
}

struct mytime {
  typedef time_t data_t;
  data_t val;
  constexpr static mytime maxtime() { return {std::numeric_limits<data_t>::max()}; }
  constexpr static mytime mintime() { return {std::numeric_limits<data_t>::min()}; }
  friend bool operator<=(const mytime &lhs, const mytime &rhs) { return lhs.val <= rhs.val; }
};
struct handle {
  void *val;
  constexpr static handle empty() { return {nullptr}; }
};

TEST(SkipListTest, TestValueRetrieve) {
  coring::skiplist_map<mytime, handle, mytime::mintime(), mytime::maxtime(), handle::empty()> list;
  auto now = std::chrono::system_clock::now();
  auto ms = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
  void *handles[205];
  int scale = 0;
  for (auto &h : handles) {
    h = reinterpret_cast<void *>(0x7fff1 + scale);
    ++scale;
  }
  // 100 node simulate expired for 100ms
  // we pretend we use up 100ms when we wake up
  // insert in reverse order to test the inserting as a bonus
  for (int i = 99; i >= 0; --i) {
    list.insert(mytime{ms + i}, handle{handles[i]});
  }
  // then we add 105 nodes won't expire (later)
  for (int i = 104; i >= 0; --i) {
    list.insert(mytime{ms + 1000 + i}, handle{handles[i + 100]});
  }
  ASSERT_TRUE(list.check_correctness());
  // pretend that we have been sleeping for 100ms
  auto new_now = ms + 100;
  auto expired_handles = list.pop_less_eq(mytime{new_now});
  ASSERT_EQ(expired_handles.size(), 100);
  ASSERT_EQ(list.size(), 105);
  ASSERT_TRUE(list.check_correctness());
  for (int i = 0; i < 100; ++i) {
    ASSERT_EQ(expired_handles[i].val, reinterpret_cast<void *>(0x7fff1 + i));
  }
  ASSERT_TRUE(list.check_correctness());
}
