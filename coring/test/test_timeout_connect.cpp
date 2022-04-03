
#include "coring/net/tcp_connection.hpp"
#include <thread>
#include <iostream>
using namespace coring;
#define catch_it                                                     \
  catch (std::exception & e) {                                       \
    std::cout << "inside root coroutine: " << e.what() << std::endl; \
    ioc->stop();                                                     \
  }                                                                  \
  static_cast<void>(0)

task<> connect(io_context *ioc) {
  using namespace std::chrono_literals;
  try {
    auto ep = net::endpoint::from_resolve("www.google.com", 80);
    std::cout << ep.address_str() << std::endl;
    [[maybe_unused]] auto c = co_await tcp::connect_to(ep, 3s);
  }
  catch_it;
}

int main() {
  io_context ctx;
  ctx.schedule(connect(&ctx));
  ctx.run();
}