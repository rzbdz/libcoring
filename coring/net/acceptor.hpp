
#ifndef CORING_ACCEPTOR_HPP
#define CORING_ACCEPTOR_HPP
#include <stop_token>
#include <iostream>
#include <unistd.h>
#include "endpoint.hpp"
#include "socket.hpp"
#include "coring/async/task.hpp"
#include "coring/io/io_context.hpp"
#include "tcp_connection.hpp"

namespace coring::tcp {
/// This class is thread-safe after it's initialization
/// a.k.a. you can accept in multiple thread
/// (linux 2.6 and later have solve the herd thundering problem)
class acceptor : noncopyable {
 private:
  void create_new_fd(net::endpoint addr);

 public:
  /// Create a acceptor by rvalue parameters.
  /// \param ip  use rvalue for convenience, or just call ctor that accept an endpoint as parameter.
  /// \param port the port to listen
  /// \param backlog see man listen(2)
  explicit acceptor(std::string &&ip, uint16_t port, int backlog = 1024)
      : backlog_{backlog}, local_addr_{std::move(ip), port} {
    create_new_fd(local_addr_);
  }
  explicit acceptor(io_context *ctx, net::endpoint addr, int backlog = 1024) : backlog_{backlog}, local_addr_{addr} {
    create_new_fd(addr);
  }
  explicit acceptor(io_context *context, std::string &&ip, uint16_t port, int backlog = 1024)
      : backlog_{backlog}, local_addr_{std::move(ip), port} {}
  explicit acceptor(net::endpoint addr, int backlog = 1024) : backlog_{backlog}, local_addr_{addr} {
    create_new_fd(addr);
  }

 public:
  /// normally we won't read or write to a listen fd, just mark it with explicit
  /// \return a file_descriptor view
  operator file_descriptor() { return {listenfd_}; }

  int fd() { return listenfd_; }

  auto get_local_endpoint() { return local_addr_; }

  void set_nonblock() { ::fcntl(listenfd_, F_SETFL, O_NONBLOCK); }

  void enable();

 public:
  template <typename CONNECTION_TYPE>
  CONNECTION_TYPE sync_accept() {
    net::endpoint peer_addr{};
    auto addr_len = net::endpoint::len;
    // sync_accept doesn't sleep on io_uring syscall,
    // we have to handle -EINTR manually.
    bool has = false;
    int connfd;
    while (!has) {
      connfd = ::accept(listenfd_, peer_addr.as_sockaddr(), &addr_len);
      if (connfd >= 0) {
        has = true;
      } else if (connfd < 0 && connfd != -EINTR) {
        throw std::system_error(std::error_code{-connfd, std::system_category()});
      }
    }
    return CONNECTION_TYPE{socket{connfd}, local_addr_, peer_addr};
  }

  task<tcp::connection> accept();

  task<tcp::peer_connection> accept_with_peer();

  task<tcp::socket_connection> accept_with_socket();

  void stop() {}

  ~acceptor() { ::close(listenfd_); }

 private:
  int listenfd_;
  int backlog_;
  net::endpoint local_addr_;
};
}  // namespace coring::tcp

#endif  // CORING_ACCEPTOR_HPP
