/// We have to use a single file to test since google test is not good for these  'infinite loop' programs
/// Or just because I don't know how to use Google test to do this.
#include "coring/utils/debug.hpp"
#include "coring/io/io_context.hpp"
#include "coring/utils/thread.hpp"
#include "coring/net/endpoint.hpp"
#include <thread>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#define LDR LOG_DEBUG_RAW
using namespace coring;
namespace client1 {
task<> reader(int fd, const std::stop_token &t) {
  int bytes_read = 0;
  auto ctx = coring::coro::get_io_context();
  auto exec = ctx->as_executor();
  for (int i = 0; !t.stop_requested();) {
    char buf[8193];
    bytes_read = co_await ctx->read(fd, buf, 8192, 0);
    i += bytes_read;
    std::cout << "bytes read:" << bytes_read << "\nnow is the " << i << " bytes in total" << std::endl;
    buf[bytes_read] = 0;
    std::cout << "data:\n" << buf << std::endl;
  }
}
/// You need a server sending specific bytes data to client
/// I suggest use python, the code I used are listed below:
/// Please use root to run it.
/// ------------------------------------------------------
/// #!/usr/bin/python2
/// from datetime import date
/// import socket
/// s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
/// s.bind(('127.0.0.1', 1025))
/// s.listen(5)
/// while True:
///     clt, adr = s.accept()
///     i = 10000
///     all = 0
///     while i > 0:
///       clt.send((str(i)).encode())
///       all += len((str(i)).encode())
///       i = i-1
///     print(all, " are sent")
/// -------------------------------------------------------
void run_client() {
  io_context ctx;
  int fd;
  struct sockaddr_in servaddr {};
  fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    abort();
  }
  bzero(&servaddr, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(1025);
  if (::inet_pton(AF_INET, "127.0.0.1", &servaddr.sin_addr) <= 0) {
    assert_eq(1, 2);
    abort();
  }
  if (connect(fd, (sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
    assert_eq(3, 4);
    abort();
  }
  std::stop_source src{};
  ctx.schedule(reader(fd, src.get_token()));
  // this simulates the signal stop, which will be supported in the future.
  std::jthread jt([&ctx, &src] {
    sleep(5);
    src.request_stop();
    sleep(1);
    ctx.stop();
    std::cout << "ask for stop...\n";
  });
  ctx.run_loop();
}
}  // namespace client1
int main() {
  client1::run_client();
  return 0;
}
