
#ifndef CORING_CONNECT_TO_HPP
#define CORING_CONNECT_TO_HPP
#pragma once
#include "coring/async/task.hpp"
#include "coring/io/io_context.hpp"
#include "endpoint.hpp"
#include "socket.hpp"

namespace coring::tcp {
template <typename CONN_TYPE>
task<CONN_TYPE> connect_to(net::endpoint local, net::endpoint peer) {
  int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    throw std::runtime_error("no resource available for socket allocation");
  }
  if (::bind(fd, local.as_sockaddr(), net::endpoint::len) < 0) {
    throw std::system_error(std::error_code{errno, std::system_category()});
  }
  int connd = co_await coro::get_io_context_ref().connect(fd, peer.as_sockaddr(), net::endpoint::len);
  co_return CONN_TYPE(fd, local, peer);
}
template <typename CONN_TYPE>
task<CONN_TYPE> connect_to(net::endpoint peer) {
  int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    throw std::runtime_error("no resource available for socket allocation");
  }
  int connd = co_await coro::get_io_context_ref().connect(fd, peer.as_sockaddr(), net::endpoint::len);
  if (connd < 0) {
    throw std::system_error(std::error_code{errno, std::system_category()});
  }
  co_return CONN_TYPE(fd);
}
template <typename CONN_TYPE>
task<CONN_TYPE> connect_to(int fd, net::endpoint local, net::endpoint peer) {
  if (::bind(fd, local.as_sockaddr(), net::endpoint::len) < 0) {
    throw std::system_error(std::error_code{errno, std::system_category()});
  }
  int connd = co_await coro::get_io_context_ref().connect(fd, peer.as_sockaddr(), net::endpoint::len);
  if (connd < 0) {
    throw std::system_error(std::error_code{errno, std::system_category()});
  }
  co_return CONN_TYPE(fd, local, peer);
}
template <typename CONN_TYPE>
task<CONN_TYPE> connect_to(int fd, net::endpoint peer) {
  int connd = co_await coro::get_io_context_ref().connect(fd, peer.as_sockaddr(), net::endpoint::len);
  if (connd < 0) {
    throw std::system_error(std::error_code{errno, std::system_category()});
  }
  co_return CONN_TYPE(fd);
}
}  // namespace coring::tcp
#endif  // CORING_CONNECT_TO_HPP
