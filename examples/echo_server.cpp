/// echo_server.cpp
/// Created by panjunzhong@outlook.com on 2022/4/13.
/// This codes use io_uring as it's used in the codes below
/// https://github.com/frevib/io_uring-echo-server/blob/master/io_uring_echo_server.c
/// so as to know the overhead of C++ & this toy library.
/// use echo-cli.py to do some basic testing,
/// use https://github.com/haraldh/rust_echo_bench for further testings

#include <iostream>
#include <chrono>

#include "coring/async_logger.hpp"
#include "coring/detail/debug.hpp"

#include "coring/async_task.hpp"
#include "coring/task.hpp"
#include "coring/acceptor.hpp"
#include "coring/timeout.hpp"
#include "coring/socket_writer.hpp"

#define catch_it               \
  catch (std::exception & e) { \
    LOG_DEBUG("{}", e.what()); \
  }                            \
  static_cast<void>(0)
#define do_nothing static_cast<void>(0)

using namespace coring;

#define MAX_CONNECTIONS 4096
#define MAX_MESSAGE_LEN 2048
#define BUFFERS_COUNT MAX_CONNECTIONS

char buf[BUFFERS_COUNT * MAX_MESSAGE_LEN] = {0};

buffer_pool::id_t GID("AB");

struct EchoServer {
  EchoServer(__u16 port) : acceptor{"0.0.0.0", port} {
    acceptor.enable();
    LOG_INFO("echo server constructed, will be run on port: {}", port);
  }

  task<> echo_loop(tcp::connection conn) {
    try {
      while (true) {
        auto read_buffer = co_await pool.read(conn.fd(), GID);
        LOG_INFO("read,bid: {} sz: {}", read_buffer->buffer_id(), read_buffer->readable());
        co_await write_all(&conn, read_buffer.get());
        LOG_INFO("written");
      }
    }
    catch_it;  // prevent async punt, just don't use co_await conn.shutdown()
    LOG_DEBUG_RAW("end of echo_loop, should close");
  }

  task<> event_loop() {
    co_await pool.provide_group_contiguous(buf, MAX_MESSAGE_LEN, BUFFERS_COUNT, GID);
    try {
      while (true) {
        LOG_TRACE("co await accept");
        auto conn = co_await acceptor.accept();
        LOG_INFO("accepted one, spawn handler");
        co_spawn(echo_loop(std::move(conn)));
      }
    }
    catch_it;
  }

  void run() {
    context.schedule(event_loop());
    context.run();
  }

  tcp::acceptor acceptor;
  buffer_pool pool{};
  io_context context{2048};
};

int main(int argc, char *argv[]) {
  __u16 port;
  bool logger_on = false;
  if (argc > 2) logger_on = true;
  if (argc > 1) {
    port = static_cast<uint16_t>(::atoi(argv[1]));
  } else {
    std::cout << "Please give a port number: ./echo_server [port: u16] [logger on: any]" << std::endl;
    exit(0);
  }
  EchoServer server{port};
  if (logger_on) {
    async_logger logger{"echo_server"};
    logger.start();
    set_log_level(INFO);  // no logging output by default
    server.run();
  } else {
    set_log_level(LOG_LEVEL_CNT);
    server.run();
  }
  return 0;
}