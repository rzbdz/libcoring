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

#define TRY_CATCH(expr)          \
  try {                          \
    expr                         \
  } catch (std::exception & e) { \
    LOG_DEBUG("{}", e.what());   \
  }                              \
  static_cast<void>(0)
#define TRY_CATCH_R(expr)          \
  try {                            \
    expr                           \
  } catch (std::exception & e) {   \
    LOG_DEBUG_RAW("%s", e.what()); \
  }                                \
  static_cast<void>(0)
//#define TRY_CATCH(expr) expr
//#define TRY_CATCH(expr) expr

using namespace coring;

#define MAX_CONNECTIONS 4096
#define BACKLOG 512
#define MAX_MESSAGE_LEN 2048
#define BUFFERS_COUNT MAX_CONNECTIONS

char buf[BUFFERS_COUNT * MAX_MESSAGE_LEN] = {0};

constexpr __u16 PORT = 22415;
buffer_pool::id_t GID{"AB"};

struct EchoServer {
  explicit EchoServer(__u16 port) : acceptor{"0.0.0.0", port} { acceptor.enable(); }

  task<> do_echo(tcp::connection conn) {
    int i = 0;
    TRY_CATCH(while (true) {
      auto &read_buffer = co_await pool.try_read_block(conn, GID, MAX_MESSAGE_LEN);
      LOG_INFO("ith: {} read,bid: {} sz: {}", i++, read_buffer.buffer_id(), read_buffer.readable());
      selected_buffer_resource on_scope_exit{read_buffer};
      auto writer = socket_writer(conn, read_buffer);
      co_await writer.write_all_to_file();
      LOG_INFO("written");
    });
    // prevent async punt, only shutdown occurs, I don't know why
    // when io_shutdown in fs/io_uring.c do handle IO_URING_F_NONBLOCK well...?
    // co_await conn.shutdown();
    // ::close(conn);
    co_await conn.close();
  }

  task<> do_accept(std::stop_token token) {
    TRY_CATCH(while (!token.stop_requested()) {
      LOG_TRACE("co await accept");
      auto conn = co_await acceptor.accept();
      LOG_INFO("accepted one, spawn handler");
      coro::spawn(do_echo(conn));
    });
  }

  void run() {
    LOG_INFO("echo server, running on port: {}", PORT);
    context.schedule(pool.provide_group_contiguous(buf, MAX_MESSAGE_LEN, BUFFERS_COUNT, GID));
    context.schedule(do_accept(src.get_token()));
    context.run();
  }

  tcp::acceptor acceptor;
  buffer_pool pool{};
  io_context context{2048};
  std::stop_source src{};
};

int main(int argc, char *argv[]) {
  __u16 port;
  bool LOGGER = false;
  if (argc > 2) {
    LOGGER = true;
  }
  if (argc > 1) {
    port = static_cast<uint16_t>(::atoi(argv[1]));
  } else {
    std::cout << "Please give a port number: ./echo_server [port: u16] [logger on: any]" << std::endl;
    exit(0);
  }
  if (LOGGER) {
    async_logger logger{"echo_server"};
    set_log_level(INFO);
    logger.run();
    EchoServer server{port};
    server.run();
  } else {
    EchoServer server{port};
    server.run();
  }
  return 0;
}