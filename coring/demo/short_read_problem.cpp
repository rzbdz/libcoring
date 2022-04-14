#include "coring/net/socket_duplexer.hpp"
#include "coring/logging/async_logger.hpp"
#include "coring/logging/logging.hpp"
#include "coring/net/tcp_connection.hpp"
#include <thread>
#include <iostream>
using namespace coring;
char request_get[] =
    "GET / HTTP/1.0\r\n"
    "Host:www.baidu.com\r\n"
    "\r\n";
char buf[56637];
task<> short_read() {
  try {
    // resolver is slow and blocking, we need a thread pool to make sure io_context won't be blocked.
    //    tcp::connection con = co_await tcp::connect_to<tcp::connection>(net::endpoint::from_resolve("www.baidu.com",
    //    80));
    tcp::peer_connection con = co_await tcp::connect_to({"127.0.0.1", 11243});
    auto t = socket_writer(con, request_get);
    co_await t.write_all_to_file();
    int ret = co_await coro::get_io_context_ref().read(con, buf, 64 * 1024, 0);
    std::cout << "I have read: " << ret << " bytes" << buf << std::endl;
    co_await con.shutdown();
    co_await con.close();
  } catch (std::exception &e) {
    std::cout << "inside short read: " << e.what() << std::endl;
  }
  std::cout << "shit end" << std::endl;
  coro::get_io_context_ref().stop();
}

int main() {
  io_context ctx;
  ctx.schedule(short_read());
  ctx.run();
}
