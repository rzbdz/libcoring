#include "coring/utils/debug.hpp"
#include "coring/io/io_context.hpp"
#include "coring/utils/thread.hpp"
#include <thread>
#include <gtest/gtest.h>
#define LDR LOG_DEBUG_RAW
using namespace coring;

TEST(UioTest, TestEventfdStop) {
  io_context ctx;

  std::jthread t1([&ctx] {
    LDR("test! %d\n go to sleep(5)", 12);
    sleep(5);
    LDR("sleep end, invoke stop");
    ctx.stop();
  });
  LDR("before run in main");
  ctx.run();
  LDR("after run in main");
  // an inf loop...
}

/// useless test
TEST(ThreadTest, TestSetDataRaw) {
  int UIO_DATA_SLOT = 0;
  void *aa, *bb, *cc;
  std::jthread a([&aa, UIO_DATA_SLOT] {
    io_context ctx;
    aa = &ctx;
    coring::thread::set_key_data(&ctx, UIO_DATA_SLOT);
    ASSERT_EQ(&ctx, coring::thread::get_key_data(UIO_DATA_SLOT));
  });

  std::jthread b([&bb, UIO_DATA_SLOT] {
    io_context ctx;
    bb = &ctx;
    coring::thread::set_key_data(&ctx, UIO_DATA_SLOT);
    ASSERT_EQ(&ctx, coring::thread::get_key_data(UIO_DATA_SLOT));
  });
  std::jthread c([&cc, UIO_DATA_SLOT] {
    io_context ctx;
    cc = &ctx;
    coring::thread::set_key_data(&ctx, UIO_DATA_SLOT);
    ASSERT_EQ(&ctx, coring::thread::get_key_data(UIO_DATA_SLOT));
  });
  ASSERT_NE(aa, bb);
  ASSERT_NE(aa, cc);
  ASSERT_NE(bb, cc);
}