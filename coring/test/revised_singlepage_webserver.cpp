// please don' t use valgrind to check memory leak.
// the io_uring syscall isn't be handled, thus induce crashes.
// I am new to http, just mark sth should be remembered here.
// How the http server know when to end a POST request reading
// need a state machine to parse the content.
// Say we have a json content or form content, just read
// until the text are well-formed.
// For example, when we have a form-data without content length,
// It can be coordinated or use content-length etc.
// If there is no content-length, bad story...
// benchmark result in wsl2 i7 6700h (linux 5.10):
// Runing info: 100 clients, running 60 sec.
// Speed=1644159 pages/min, 1808574 bytes/sec.
// Requests: 1644159 susceed, 0 failed.
// Actually I should compare different server, like epoll
// and boost asio, I will do that after the whole picture
// settle down and bugs get fixed.
#include "coring/net/acceptor.hpp"
#include "coring/net/socket_duplexer.hpp"
#include "coring/io/timeout.hpp"
#include <thread>
#include <iostream>
using namespace coring;
// hello-world for benchmark
constexpr char response200[] =
    "HTTP/1.0 200\r\n"
    "Content-type: text/html\r\n"
    "\r\n"
    "<html>HELLO WORLD</html>";

task<> send_200_then_quit(tcp::connection conn) {
  try {
    auto wrapper = socket_reader(conn, sizeof(response200));
    [[maybe_unused]] auto str = co_await wrapper.read_till_2crlf();
    co_await socket_writer(conn, response200).write_all_to_file();
  } catch (std::exception &e) {
    std::cout << "inside read" << e.what() << std::endl;
  }
  conn.shutdown();
  conn.close();
}
task<> acceptor(std::stop_token tk, tcp::acceptor &act) {
  try {
    while (tk.stop_requested() == false) {
      auto conn = co_await act.accept();
      coro::spawn(send_200_then_quit(std::move(conn)));
    }
  } catch (std::exception &e) {
    std::cout << "inside accept: " << e.what() << std::endl;
  }
}
void setup_acceptor(io_context *ctx, tcp::acceptor &act, std::stop_token tk) {
  ctx->register_files({act.fd()});
  for (int i = 0; i < 4; i++) {
    ctx[i].schedule(acceptor(tk, act));
  }
}
int main(int argc, char *argv[]) {
  io_context ctx[4];
  __u16 port = 11243;
  if (argc > 1) {
    port = static_cast<uint16_t>(::atoi(argv[1]));
    std::cout << "port is: " << port << ", server stated, no logger this time" << std::endl;
  }
  tcp::acceptor act{"0.0.0.0", port};
  act.enable();
  std::stop_source src;
  setup_acceptor(ctx, act, src.get_token());
  // thread pool isn't work yet, just punt it manually.
  std::latch lt{1};
  std::jthread jtx1([&]() { ctx[0].run(lt); });
  std::jthread jtx2([&]() { ctx[1].run(lt); });
  std::jthread jtx3([&]() { ctx[2].run(lt); });
  std::jthread jtx4([&]() { ctx[3].run(lt); });
  return 0;
}

// # An old version
// But I found that we don't want to sleep the main thread on here
// Because this spawn is slow , we should use multiple thread accepting concurrently.
// avoiding this slow performance.
// I think we should use a lock-free queue for the task queue,
// during an interview, the interviewer asked me why I didn't use
// lock-free queue for this kind of queues just like what I did in
// the async logger....
// But the problem I think is due to the 'single' restriction, which
// means we need more thread_local memory leakage, so spsc for async
// logger should not be used. Actually,
// I think we can use.............. the rest notes I just move to the spawn
// -------------------------------------------------------------------------------
//  auto tk = src.get_token();
//  try {
//    int count = 0;
//    while (tk.stop_requested() == false) {
//      auto conn = act.sync_accept<tcp::connection>();
//      ctx[count].spawn<true>(send_200_then_quit(std::move(conn)));
//      count = (count + 1) % 2;
//    }
//  } catch (std::exception &e) {
//    std::cout << "inside accept: " << e.what() << std::endl;
//  }
// --------------------------------------------------------------------------------