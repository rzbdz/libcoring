/// echo_server.cpp
/// Created by panjunzhong@outlook.com on 2022/4/13.
/// This codes use io_uring as it's used in the codes below
/// https://github.com/frevib/io_uring-echo-server/blob/master/io_uring_echo_server.c
/// so as to know the overhead of C++ & this toy library.
/// use echo-cli.py to do some basic testing,
/// use https://github.com/haraldh/rust_echo_bench for further testings

#include <iostream>
#include <chrono>

#include "coring/logging/async_logger.hpp"

#include "coring/async/async_task.hpp"
#include "coring/async/task.hpp"
#include "coring/utils/debug.hpp"
#include "coring/net/acceptor.hpp"
#include "coring/io/timeout.hpp"
#include "coring/net/socket_writer.hpp"

#define catch_it_then(then)    \
  catch (std::exception & e) { \
    LOG_DEBUG("{}", e.what()); \
  }                            \
  then;                        \
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
        auto &read_buffer = co_await pool.try_read_block(conn, GID);
        LOG_INFO("read,bid: {} sz: {}", read_buffer.buffer_id(), read_buffer.readable());
        selected_buffer_resource on_scope_exit{read_buffer};
        auto writer = socket_writer(conn, read_buffer);
        co_await writer.write_all_to_file();
        LOG_INFO("written");
      }
    }
    catch_it_then(co_await conn.close());  // prevent async punt, just don't use co_await conn.shutdown()
  }

  task<> event_loop() {
    co_await pool.provide_group_contiguous(buf, MAX_MESSAGE_LEN, BUFFERS_COUNT, GID);
    try {
      while (true) {
        LOG_TRACE("co await accept");
        auto conn = co_await acceptor.accept();
        LOG_INFO("accepted one, spawn handler");
        coro::spawn(echo_loop(conn));
      }
    }
    catch_it_then(do_nothing);
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
    set_log_level(TRACE);  // no logging output by default
    server.run();
  } else {
    server.run();
  }
  return 0;
}