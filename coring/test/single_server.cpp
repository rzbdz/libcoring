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
#include <iostream>
using namespace coring;
// hello-world for benchmark
constexpr char response200[] =
    "HTTP/1.0 200\r\n"
    "Content-type: text/html\r\n"
    "\r\n"
    "<html>HELLO WORLD</html>"
    "<html>HELLO WORLD</html>"
    "<html>HELLO WORLD</html>"
    "<html>HELLO WORLD</html>"
    "<html>HELLO WORLD</html>"
    "<html>HELLO WORLD</html>"
    "<html>HELLO WORLD</html>"
    "<html>HELLO WORLD</html>"
    "<html>HELLO WORLD</html>";

task<> send_200_then_quit(tcp::connection conn) {
  try {
    auto wrapper = socket_reader(conn, 2048);
    //    [[maybe_unused]] auto str = co_await wrapper.read_till_2crlf();
    co_await wrapper.read_some();
    co_await socket_writer(conn, response200).write_all_to_file();
  } catch (std::exception &e) {
    std::cout << "inside read " << e.what() << std::endl;
  }
  // prevent async punt, only shutdown occurs, I don't know why
  // when io_shutdown in fs/io_uring.c do handle IO_URING_F_NONBLOCK well...?
  // co_await conn.shutdown();
  // ::close(conn);
  co_await conn.close();
  // ::close(conn);
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
  std::cout << "this acceptor have handled: " << how_many << std::endl;
}
int main(int argc, char *argv[]) {
  if (argc == 1) {
    std::cout << "Single Server testing, no http parsing overhead, usage ({} is param):\n"
                 "  ./single_server {port} {timeout} {1 with sq-poll on} "
              << std::endl;
    return 0;
  }
  constexpr auto ENT = 2048;
  bool sq_poll = false;
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
  if (argc > 3) {
    int tmp = static_cast<uint16_t>(::atoi(argv[3]));
    if (tmp == 1) {
      sq_poll = true;
    }
  }
  uint32_t fl = 0;
  if (sq_poll) {
    fl = IORING_SETUP_SQPOLL;
  }
  io_context big_bother{ENT, fl};
  std::cout << "single_server, sq-poll: " << (sq_poll ? "on" : "off") << " (threads: 1)" << std::endl;
  tcp::acceptor act{"0.0.0.0", port};
  act.enable();
  std::stop_source src;
  io_context *ctx[] = {&big_bother};
  ctx[0]->schedule([](std::stop_source *spt, long s_time) -> task<> {
    using namespace std::chrono_literals;
    co_await timeout(std::chrono::seconds(s_time));
    spt->request_stop();
    coro::get_io_context_ref().stop();
  }(&src, s_time));
  // This would bring async punts too, at about 1000+ iou-wrk thread.
  ctx[0]->schedule(acceptor(src.get_token(), act));
  //  ctx[0]->schedule(acceptor(src.get_token(), act));
  //  ctx[0]->schedule(acceptor(src.get_token(), act));
  std::latch lt{1};
  ctx[0]->run(lt);
  return 0;
}
