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
// Actually I should do benchmarks compare different server, like epoll
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
  co_await conn.shutdown();
  co_await conn.close();
}
task<> acceptor(std::stop_token tk, tcp::acceptor &act) {
  int how_many = 0;
  try {
    while (tk.stop_requested() == false) {
      auto conn = co_await act.accept();
      coro::spawn(send_200_then_quit(std::move(conn)));
      how_many++;
    }
  } catch (std::exception &e) {
    std::cout << "inside accept: " << e.what() << "listen fd: " << act.fd() << std::endl;
  }
  std::cout << "this acceptor have handle: " << how_many << std::endl;
}
void setup_acceptor(io_context **ctx, tcp::acceptor &act, std::stop_token tk, long s_time) {
  for (int i = 0; i < 4; i++) {
    // Just for fun?
    // ctx[i]->register_files({act.fd()});
    ctx[i]->schedule(acceptor(tk, act));
    ctx[i]->schedule(acceptor(tk, act));
    ctx[i]->schedule(acceptor(tk, act));
    ctx[i]->schedule(acceptor(tk, act));
    ctx[i]->schedule([](long s_time) -> task<> {
      co_await timeout(std::chrono::seconds(s_time));
      coro::get_io_context_ref().stop();
    }(s_time));
  }
}
int main(int argc, char *argv[]) {
  constexpr auto ENT = 128;
  io_context big_bother{ENT, IORING_SETUP_SQPOLL};
  io_uring_params params{
      .flags = IORING_SETUP_SQPOLL | IORING_SETUP_ATTACH_WQ,
      .sq_thread_idle = 3000,
      .wq_fd = static_cast<__u32>(big_bother.ring_fd()),
  };
  io_context rest[3] = {{ENT, &params}, {ENT, &params}, {ENT, &params}};
  // io_context rest[4];
  __u16 port = 11243;
  long s_time = 100;
  if (argc > 1) {
    port = static_cast<uint16_t>(::atoi(argv[1]));
    std::cout << "port is: " << port << ", server stated, no logger this time" << std::endl;
  }
  if (argc > 2) {
    s_time = static_cast<uint16_t>(::atoi(argv[2]));
    std::cout << "timeout is: " << s_time << std::endl;
  }
  tcp::acceptor act{"0.0.0.0", port};
  act.enable();
  std::stop_source src;
  io_context *ctx[] = {&big_bother, &rest[0], &rest[1], &rest[2]};
  ctx[3]->schedule([](std::stop_source *spt, long s_time) -> task<> {
    using namespace std::chrono_literals;
    co_await timeout(std::chrono::seconds(s_time));
    spt->request_stop();
    coro::get_io_context_ref().stop();
  }(&src, s_time));
  setup_acceptor(ctx, act, src.get_token(), s_time);
  // thread pool isn't work yet, just punt it manually.
  std::latch lt{1};
  std::jthread jtx1([&]() { ctx[0]->run(lt); });
  std::jthread jtx2([&]() { ctx[1]->run(lt); });
  std::jthread jtx3([&]() { ctx[2]->run(lt); });
  ctx[3]->run(lt);
  return 0;
}
