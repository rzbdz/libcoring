
#include <thread>
#include <iostream>

#include "coring/net/tcp_connection.hpp"
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
    io_cancel_source src;
    auto ep = net::endpoint::from_resolve("www.google.com", 80);
    std::cout << ep.address_str() << std::endl;
    std::cout << "eager start connect_to" << std::endl;
    auto promise = tcp::connect_to(ep, src.get_token());
    std::cout << "connect to is setup, we then start a separate timeout" << std::endl;
    // use this for simplicity, user space timer is low cost-effective here...
    co_await ioc->timeout(3s);
    std::cout << "oops, timeout, let's check if the connect task successes" << std::endl;
    if (!promise.is_ready()) {
      std::cout << "Sad story, the promise is still not ready, we then issue a cancellation request" << std::endl;
      auto res = co_await src.cancel_and_wait_for_result(*ioc);
      std::cout << "cancel request returns: "
                << (res == 0 ? "cancel success" : (res == -EALREADY ? "EALREADY" : "ERROR")) << std::endl;
    } else {
      std::cout << "Good news, the promise is ready, we can get the result" << std::endl;
    }
    std::cout << "Anyway, it doesn't matter if the promise is cancelled, we can co_await it now, it might throw, we "
                 "just catch it"
              << std::endl;
    [[maybe_unused]] auto c = co_await promise;
  }
  catch_it;
}

int main() {
  io_context ctx;
  ctx.schedule(connect(&ctx));
  ctx.run();
}