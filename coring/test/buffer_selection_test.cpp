
#include "coring/async/async_task.hpp"
#include "coring/async/task.hpp"
#include "coring/utils/debug.hpp"
// #define LDR LOG_DEBUG_RAW // turn on raw debugging loggings
#define LDR(fmt, args...) static_cast<void>(0)
#include <gtest/gtest.h>
#define NO_IO_CONTEXT
#include "coring/net/buffer_pool.hpp"
#include <thread>
#include <chrono>
// #define PRINT_ALL_BUFFERS // turn on mock io_buffer_selection printings
using namespace coring;
using nanoseconds = std::chrono::nanoseconds;
using microseconds = std::chrono::microseconds;
using system_clock = std::chrono::system_clock;
constexpr __u16 port{65421};
struct MockService {
  struct fake_context {
    char *base{nullptr};
    int many{0};
    int len{0};
    std::vector<bool> used;
    __u16 myname = 0;
    void print_all() {
#ifdef PRINT_ALL_BUFFERS
      LDR("base: %p, len: %d, many: %d,  g: %d", base, len, many, myname);
      int i = 0;
      for (auto b : used) {
        LDR("%d is %s", i, used[i] ? "used" : "avaliable");
        i++;
      }
#endif
    }
    async_task<int> provide_buffers(char *b, int n, int m, __u16 gname, __u16);
    int cursor = 3;
    bool NOBUF = false;
    bool go(int first);
    task<std::pair<int, int>> read_buffer_select(int fd, __u16 gname, int n, int off);
  };
  static fake_context impl_;
  static fake_context &get_io_context_ref() { return impl_; }
};

MockService::fake_context MockService::impl_{};

async_task<> SimpleProvideOneGroup(buffer_pool_base<MockService> &pool);

////////////////////////////GTests/////////////////////////////
////////////////////////////GTests/////////////////////////////
////////////////////////////GTests/////////////////////////////
////////////////////////////GTests/////////////////////////////
////////////////////////////GTests/////////////////////////////
////////////////////////////GTests/////////////////////////////
////////////////////////////GTests/////////////////////////////
TEST(BufferSelection, SimpleProvideOneGroup) {
  buffer_pool_base<MockService> pool;
  // return immediately
  // no real awaitable is really awaited.
  SimpleProvideOneGroup(pool);
}

async_task<int> MockService::fake_context::provide_buffers(char *b, int n, int m, __u16 gname, __u16) {
  if (gname == myname) {
    LDR("return buffer: ");
    auto index = (b - base) / len;
    int cir = 0;
    for (auto i = index; cir++ < m; i++) {
      LDR("buffer at %ld enabled", i);
      used[i] = false;
    }
  } else {
    len = n;
    many = m;
    base = b;
    used = std::vector<bool>(many);
    myname = gname;
    LDR("first provide: ");
  }
  print_all();
  co_return 100;
}
task<std::pair<int, int>> MockService::fake_context::read_buffer_select(int fd, __u16 gname, int n, int off) {
  auto tmp = cursor;
  while (go(tmp))
    if (NOBUF) co_return std::make_pair(-ENOBUFS, 0);
  used[cursor] = true;
  print_all();
  co_return std::make_pair(100, cursor);
}
bool MockService::fake_context::go(int first) {
  if ((++cursor) == many) cursor = 0;
  if (cursor == first) NOBUF = true;
  return used[cursor];
}
async_task<> SimpleProvideOneGroup(buffer_pool_base<MockService> &pool) {
  char *ptr = (char *)(0x7A8000);
  int len = 0x8000;
  int many = 0xA;
  co_await pool.provide_group_contiguous(ptr, len, many, "AC");
  {
    auto buffer_view = selected_buffer_resource<MockService>{co_await pool.try_read_block(0, "AC")};
    auto &buffer = buffer_view.get();
    LDR("%ld", buffer.size());
    // only expect can be used...
    EXPECT_EQ(buffer.capacity(), 0x8000);
  }
  for (auto i = 0; i < many; i++) {
    try {
      [[maybe_unused]] auto &bf = co_await pool.try_read_block(0, "AC");
    } catch (std::system_error &e) {
      LDR("%s", e.what());
      EXPECT_EQ("Should not run out for a resource managed", "thrown");
    }
  }
  try {
    [[maybe_unused]] auto &bf = co_await pool.try_read_block(0, "AC");
  } catch (std::system_error &e) {
    LDR("%s", e.what());
    EXPECT_EQ("Should run-out", "Should run-out");
  }
  co_return;
}
