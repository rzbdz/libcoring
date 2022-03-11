#include "coring/utils/debug.hpp"
#include "coring/io/io_context.hpp"
#include "coring/utils/thread.hpp"
#include <thread>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define LDR LOG_DEBUG_RAW
using namespace coring;

TEST(IO_CONTEXT, TestEventfdStop) {
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