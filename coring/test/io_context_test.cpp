#include "coring/utils/debug.hpp"
#include "coring/io/io_context.hpp"
#include "coring/utils/thread.hpp"
#include "coring/io/timeout.hpp"
#include <thread>
#include <chrono>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <arpa/inet.h>
// #define LDR LOG_DEBUG_RAW
#define LDR(fmt, args...) static_cast<void>(0)
using namespace coring;
using nanoseconds = std::chrono::nanoseconds;
using microseconds = std::chrono::microseconds;
using system_clock = std::chrono::system_clock;
// use --gtest_filter=''
TEST(Run, EventfdStop) {
  io_context ctx;
  std::jthread t1([&ctx] {
    // LDR("test! %d\n go to sleep(5)", 12);
    sleep(5);
    // LDR("sleep end, invoke stop");
    ctx.stop();
  });
  // LDR("before run in main");
  ctx.run();
  // LDR("after run in main");
  // an inf loop...
  // Not loop inf would be considered passed...
}

task<> sleep_for(std::chrono::microseconds t) {
  auto stamp_before = std::chrono::duration_cast<microseconds>(system_clock::now().time_since_epoch());
  co_await timeout(t);
  auto stamp_after = std::chrono::duration_cast<microseconds>(system_clock::now().time_since_epoch());
  auto pass = (stamp_after - stamp_before).count();
  auto ti = t.count();
  // LDR("It now time passed? %ldus, oder: %d, %ldus from my count", t.count(), local_order, pass);
  // I don' t know why gtest assertion would return sth...
  // error rate
  // absolute error, 500us = 0.5ms
  if ((std::abs(ti - pass)) >= 500) {
    EXPECT_LE(static_cast<double>(std::abs(ti - pass)) / ti, 0.1);
  }
}

TEST(Timeout, sleep3s) {
  io_context ctx;
  auto exec = ctx.as_executor();
  using namespace std::chrono_literals;
  schedule(exec, sleep_for(3s));
  std::jthread t1([&ctx] {
    // LDR("test! %d\n go to sleep(5)", 12);
    sleep(6);
    // LDR("sleep end, invoke stop");
    ctx.stop();
  });
  ctx.run();
}
TEST(Timeout, sleep100us) {
  io_context ctx;
  auto exec = ctx.as_executor();
  using namespace std::chrono_literals;
  schedule(exec, sleep_for(100us));
  std::jthread t1([&ctx] {
    LDR("test! %d\n go to sleep(5)", 12);
    sleep(2);
    LDR("sleep end, invoke stop");
    ctx.stop();
  });
  ctx.run();
}

TEST(Timeout, multisleep) {
  io_context ctx;
  auto exec = ctx.as_executor();
  using namespace std::chrono_literals;
  schedule(exec, []() -> task<> {
    co_await timeout(3s);
    LDR("3s wakeup, should be #3");
  }());
  schedule(exec, []() -> task<> {
    co_await timeout(400us);
    LDR("400us wakeup, should be #1");
  }());
  schedule(exec, []() -> task<> {
    co_await timeout(20ms);
    LDR("20ms wakeup, should be #2");
  }());
  schedule(exec, []() -> task<> {
    co_await timeout(6s);
    LDR("6s wakeup, should be #4");
    co_await timeout(10s);
    LDR("6s then 10s wakeup, should be #5");
    co_await timeout(500ms);
    LDR("6s,10s then 500mss wakeup, should be #6");
  }());
  std::jthread t1([&ctx] {
    sleep(20);
    ctx.stop();
  });
  ctx.run();
}
int counter = 0;
int order = 0;
task<> sleep_for2(std::chrono::microseconds t) {
  // int local_order = order;
  ++order;
  auto stamp_before = std::chrono::duration_cast<microseconds>(system_clock::now().time_since_epoch());
  co_await timeout(t);
  auto stamp_after = std::chrono::duration_cast<microseconds>(system_clock::now().time_since_epoch());
  auto pass = (stamp_after - stamp_before).count();
  auto ti = t.count();
  // LDR("It now time passed? %ldus, oder: %d, %ldus from my count", t.count(), local_order, pass);
  // I don' t know why gtest assertion would return sth...
  // error rate
  // absolute error, 500us = 0.5ms
  if ((std::abs(ti - pass)) >= 500) {
    EXPECT_LE(static_cast<double>(std::abs(ti - pass)) / ti, 0.1);
  }
  counter++;
}
TEST(Timeout, manyTimers) {
  counter = 0;
  order = 0;
  io_context ctx;
  auto exec = ctx.as_executor();
  using namespace std::chrono_literals;
  schedule(exec, sleep_for2(1s));
  schedule(exec, sleep_for2(1s));
  schedule(exec, sleep_for2(5s));
  schedule(exec, sleep_for2(1s));
  schedule(exec, sleep_for2(1s));
  schedule(exec, sleep_for2(500us));
  schedule(exec, sleep_for2(1s));
  schedule(exec, sleep_for2(500ms));
  schedule(exec, sleep_for2(1s));
  schedule(exec, sleep_for2(1s));
  schedule(exec, sleep_for2(600ms));
  schedule(exec, sleep_for2(700ms));
  schedule(exec, sleep_for2(200ms));
  schedule(exec, sleep_for2(1s));
  schedule(exec, sleep_for2(3s));
  schedule(exec, sleep_for2(3s));
  schedule(exec, sleep_for2(3s));
  schedule(exec, sleep_for2(1s));
  schedule(exec, sleep_for2(500us));
  schedule(exec, sleep_for2(1s));
  schedule(exec, sleep_for2(500ms));
  schedule(exec, sleep_for2(1s));
  schedule(exec, sleep_for2(1s));
  schedule(exec, sleep_for2(600ms));
  schedule(exec, sleep_for2(700ms));
  schedule(exec, sleep_for2(200ms));
  schedule(exec, sleep_for2(1s));
  schedule(exec, sleep_for2(1s));
  schedule(exec, sleep_for2(12s));
  std::jthread t1([&ctx] {
    sleep(20);
    ctx.stop();
  });
  ctx.run();
  ASSERT_EQ(counter, 29);
  counter = 0;
  order = 0;
}

TEST(Timeout, tremendousTimers) {
  counter = 0;
  order = 0;
  io_context ctx;
  auto exec = ctx.as_executor();
  using namespace std::chrono_literals;
  for (auto i = 0; i < 100; i++) {
    schedule(exec, sleep_for2(100ms));
    schedule(exec, sleep_for2(2s));
    schedule(exec, sleep_for2(300ms));
    schedule(exec, sleep_for2(20ms));
    schedule(exec, sleep_for2(5s));
  }
  std::jthread t1([&ctx] {
    sleep(10);
    ctx.stop();
  });
  ctx.run();
  ASSERT_EQ(counter, 500);
  counter = 0;
  order = 0;
}
TEST(Timeout, recursiveTimers) {
  io_context ctx;
  auto exec = ctx.as_executor();
  using namespace std::chrono_literals;
  schedule(exec, []() -> task<> {
    for (int i = 0; i < 1000; i++) {
      co_await sleep_for(i * 100ms);
    }
  }());
  std::jthread t1([&ctx] {
    sleep(20);
    ctx.stop();
  });
  ctx.run();
}