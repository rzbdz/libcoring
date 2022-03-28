// please don' t use valgrind to check memory leak.
// the io_uring syscall isn't be handled, thus induce crashes.
#include "coring/net/acceptor.hpp"
#include "coring/net/buffered.hpp"
#include "coring/logging/async_logger.hpp"
#include "coring/logging/logging.hpp"
#include <thread>
#include <iostream>
using namespace coring;
// I am new to http, just mark sth should be remembered here.
// How the http server know when to end a POST request reading
// need a state machine to parse the content.
// Say we have a json content or form content, just read
// until the text are well-formed.
// For example, when we have a form-data without content length,
// It can be coordinated or use content-length etc.
// If there is no content-length, bad story...
constexpr char response200[] =
    "HTTP/1.0 200\r\n"
    "Content-type: text/html\r\n"
    "\r\n"
    "<html>"
    "<head>"
    "<title>libcoring 404 Testing page...</title>"
    "</head>"
    "<body>"
    "<h1>Good Request, but bad server.</h1>"
    "<p>Glad that the test page work, just take a break...</p>"
    "<a href=\"https://github.com/rzbdz/libcoring/\">GitHub Page of libcoring</a><br>"
    "<a href=\"https://blog.csdn.net/u010180372\">CSDN Blog</a><br>"
    "</body>"
    "</html>";

task<> send_200_then_quit(tcp::connection conn, int &count) {
  count++;
  auto wrapper = io::buffered(conn);
  try {
    for (;;) {
      auto str = co_await wrapper.read_crlf_line();
      LOG_DEBUG("read a line: {}", str);
      if (str.size() >= 2 && (str[0] == '\r' && str[1] == '\n')) {
        co_await wrapper.write_some(response200, sizeof(response200));
        break;
      }
    }
  } catch (std::exception &e) {
    LOG_DEBUG("Connector bad write, msg: {}", e.what());
  }
  count--;
  LOG_DEBUG("Ready to quit a client");
  conn.shutdown();
  conn.close();
}

task<> line_server(tcp::acceptor &act, std::stop_token tk) {
  int count = 0;
  try {
    while (tk.stop_requested() == false) {
      auto conn = co_await act.accept();
      schedule(coro::get_io_context_ref(), send_200_then_quit(std::move(conn), count));
      LOG_DEBUG("a client come in, conn error: {}, socket fd: {}, now the clients: {}", conn.error(), conn.fd(), count);
    }
  } catch (std::exception &e) {
    LOG_DEBUG("Connector bad write, msg: {}", e.what());
    coro::get_io_context_ref().stop();
  }
}

int main() {
  io_context ctx;
  async_logger as{"tcp_line_server"};
  as.run();
  LOG_DEBUG("first log");
  tcp::acceptor act{"0.0.0.0", 11243};
  act.enable();
  std::stop_source src;
  schedule(ctx, line_server(act, src.get_token()));
  std::jthread th1([&]() {
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(20s);
    src.request_stop();
    act.stop();
    as.stop();
    ctx.stop();
  });
  ctx.run();
  return 0;
}