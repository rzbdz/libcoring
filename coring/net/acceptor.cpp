
#include "acceptor.hpp"

void coring::tcp::acceptor::create_new_fd(coring::net::endpoint addr) {
  int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    throw std::runtime_error("no resource available for socket allocation");
  }
  listenfd_ = fd;
  if (::bind(listenfd_, addr.as_sockaddr(), net::endpoint::len) < 0) {
    throw std::runtime_error("cannot bind port or sth wrong with the socket fd");
  }
}
coring::task<coring::tcp::connection> coring::tcp::acceptor::accept() {
  auto &ctx = coro::get_io_context();
  net::endpoint peer_addr{};
  auto addr_len = net::endpoint::len;
  auto connfd = co_await ctx.accept(listenfd_, peer_addr.as_sockaddr(), &addr_len);
  if (connfd < 0) {
    throw std::system_error(std::error_code{-connfd, std::system_category()});
  }
  co_return tcp::connection{socket{connfd}};
}
coring::task<coring::tcp::peer_connection> coring::tcp::acceptor::accept_with_peer() {
  auto &ctx = coro::get_io_context();
  net::endpoint peer_addr{};
  auto addr_len = net::endpoint::len;
  auto connfd = co_await ctx.accept(listenfd_, peer_addr.as_sockaddr(), &addr_len);
  if (connfd < 0) {
    throw std::system_error(std::error_code{-connfd, std::system_category()});
  }
  co_return tcp::peer_connection{socket{connfd}, peer_addr};
}
coring::task<coring::tcp::socket_connection> coring::tcp::acceptor::accept_with_socket() {
  auto &ctx = coro::get_io_context();
  net::endpoint peer_addr{};
  auto addr_len = net::endpoint::len;
  auto connfd = co_await ctx.accept(listenfd_, peer_addr.as_sockaddr(), &addr_len);
  if (connfd < 0) {
    throw std::system_error(std::error_code{-connfd, std::system_category()});
  }
  co_return tcp::socket_connection{socket{connfd}, local_addr_, peer_addr};
}
void coring::tcp::acceptor::enable() { ::listen(listenfd_, backlog_); }
