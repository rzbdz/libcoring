// We have to use a single file to test since google test is not good for these  'infinite loop' programs
// Or just because I don't know how to use Google test to do this.
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
namespace server1 {
char test[] = "abadadslfasfafsffasffafafaffffafafa16446sa4dgdasgsagsdgsdag---end";
auto acceptor(int fd, const std::stop_source &src, io_cancel_token *cancel_tk) -> task<> {
  auto ctx = coro::get_io_context();
  auto exec = ctx->as_executor();
  // Do not forget that parameters of accept is both input and output...
  // I follow the rule that use const ref as input, use ptr when modification is necessary.
  // As for the system call, just remember the input parameter would be cosnt T*.
  socket::endpoint peeraddr{};
  auto addrlen = socket::endpoint::len;
  auto alp = &addrlen;
  auto pa = peeraddr.as_sockaddr();
  for (; src.stop_requested() == false;) {
    auto act = ctx->accept(fd, pa, alp);
    *cancel_tk = act.get_cancel_token();
    int connfd = co_await act;
    execute(exec, [ctx](int cfd) -> task<> {
      auto b = 0u;
      while (b < 66) {
        b += co_await ctx->write(cfd, test, 66, 0);
      }
    }(connfd));
  }
}

void run_server() {
  io_context ctx{};
  int fd;
  if ((fd = ::socket(AF_INET, SOCK_STREAM, 0)) < 0) abort();  // NOLINT
  socket::endpoint servaddr("127.0.0.1", 1025);
  [[maybe_unused]] auto bi = ::bind(fd, servaddr.as_sockaddr(), sizeof(servaddr));
  ::listen(fd, 5);
  std::stop_source src{};
  // Not finished yet -- cancellation support.
  // Just use stop_source + stop_token...
  io_cancel_token cancel;
  ctx.schedule(acceptor(fd, src, &cancel));
  ctx.run_loop();
}
}  // namespace server1
int main() {
  server1::run_server();
  return 0;
}