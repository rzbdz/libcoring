
#ifndef CORING_ACCEPTOR_HPP
#define CORING_ACCEPTOR_HPP
#include <stop_token>
#include <iostream>
#include "endpoint.hpp"
#include "socket.hpp"
#include "coring/async/task.hpp"
#include "coring/io/io_context.hpp"
#include "tcp_connection.hpp"
namespace coring::tcp {
class acceptor : noncopyable {
 private:
  void create_new_fd(net::endpoint addr);

 public:
  /// Create a acceptor by rvalue parameters.
  /// \param ip  use rvalue for convenience, or just call ctor that accept an endpoint as parameter.
  /// \param port the port to listen
  /// \param backlog see man listen(2)
  explicit acceptor(std::string &&ip, uint16_t port, int backlog = 1024)
      : backlog_{backlog}, local_addr_{std::move(ip), port}, ctx_cache_{coro::get_io_context_ref()} {
    create_new_fd(local_addr_);
  }
  explicit acceptor(io_context &ctx, net::endpoint addr, int backlog = 1024)
      : backlog_{backlog}, local_addr_{addr}, ctx_cache_{ctx} {
    create_new_fd(addr);
  }
  explicit acceptor(io_context &context, std::string &&ip, uint16_t port, int backlog = 1024)
      : backlog_{backlog}, local_addr_{std::move(ip), port}, ctx_cache_{context} {}
  explicit acceptor(net::endpoint addr, int backlog = 1024)
      : backlog_{backlog}, local_addr_{addr}, ctx_cache_{coro::get_io_context_ref()} {
    create_new_fd(addr);
  }

 public:
  /// normally we won't read or write to a listen fd, just mark it with explicit
  /// \return a file_descriptor view
  explicit operator file_descriptor() { return {listenfd_}; }

  auto get_local_endpoint() { return local_addr_; }

  void enable();

 public:
  task<tcp::connection> accept();

  task<tcp::peer_connection> accept_with_peer();

  task<tcp::socket_connection> accept_with_socket();

  void stop() {}

  ~acceptor() { ::close(listenfd_); }

 private:
  int listenfd_;
  int backlog_;
  net::endpoint local_addr_;
  io_context &ctx_cache_;
};
}  // namespace coring::tcp

#endif  // CORING_ACCEPTOR_HPP
