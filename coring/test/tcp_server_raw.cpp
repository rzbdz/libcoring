// We have to use a single file to test since google test is not good for these  'infinite loop' programs
// Or just because I don't know how to use Google test to do this.
#include "coring/utils/debug.hpp"
#include "coring/io/io_context.hpp"
#include "coring/utils/thread.hpp"
#include "coring/net/endpoint.hpp"
#include "coring/net/async_buffer.hpp"
#include <thread>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#ifndef LDR
#define LDR LOG_DEBUG_RAW
#endif
using namespace coring;
namespace server1 {
void generate_chunk(async_buffer &asb) {
  asb.push_back_string("#64b2#len#55#DTATADKANGKASDNGDASKNGKDASNGKNSADKGNQWIKEGNDSKAGNAKSDGBNASKDG#END#\n");
  asb.push_back_string("#64b2#len#55#DTATADKANGKASDNGDASKNGKDASNGKNSADKGNQWIKEGNDSKAGNAKSDGBNASKDG#END#\n");
  asb.push_back_string("#64b2#len#55#DTATADKANGKASDNGDASKNGKDASNGKNSADKGNQWIKEGNDSKAGNAKSDGBNASKDG#END#\n");
  asb.push_back_string("#64b2#len#55#DTATADKANGKASDNGDASKNGKDASNGKNSADKGNQWIKEGNDSKAGNAKSDGBNASKDG#END#\n");
  asb.push_back_string("#TRUE_END#!!MAKE-SURE-YOU-SEE-THIS!!#\n");
}
auto acceptor(int fd, const std::stop_source &src, io_cancel_token *cancel_tk) -> task<> {
  auto ctx = coro::get_io_context();
  auto exec = ctx->as_executor();
  // Do not forget that parameters of accept is both input and output...
  // I follow the rule that use const ref as input, use ptr when modification is necessary.
  // As for the system call, just remember the input parameter would be cosnt T*.
  net::endpoint peeraddr{};
  auto addrlen = net::endpoint::len;
  auto alp = &addrlen;
  auto pa = peeraddr.as_sockaddr();
  for (; src.stop_requested() == false;) {
    auto act = ctx->accept(fd, pa, alp);
    *cancel_tk = act.get_cancel_token();
    int connfd = co_await act;
    execute(exec, [ctx](int cfd) -> task<> {
      async_buffer bf;
      generate_chunk(bf);
      co_await bf.write_all_to_file(cfd);
    }(connfd));
  }
}

void run_server() {
  io_context ctx{};
  int fd;
  if ((fd = ::socket(AF_INET, SOCK_STREAM, 0)) < 0) abort();  // NOLINT
  net::endpoint servaddr("127.0.0.1", 1025);
  [[maybe_unused]] auto bi = ::bind(fd, servaddr.as_sockaddr(), sizeof(servaddr));
  ::listen(fd, 5);
  std::stop_source src{};
  // Not finished yet -- cancellation support.
  // Just use stop_source + stop_token...
  io_cancel_token cancel;
  ctx.schedule(acceptor(fd, src, &cancel));
  ctx.run();
}
}  // namespace server1
int main() {
  server1::run_server();
  return 0;
}