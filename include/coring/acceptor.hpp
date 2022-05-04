
#ifndef CORING_ACCEPTOR_HPP
#define CORING_ACCEPTOR_HPP
#include <stop_token>
#include <iostream>
#include <unistd.h>
#include "endpoint.hpp"
#include "socket.hpp"
#include "task.hpp"
#include "io_context.hpp"
#include "tcp_connection.hpp"

namespace coring::tcp {
/// This class is thread-safe after it's initialization
/// a.k.a. you can accept in multiple thread
/// (linux 2.6 and later have solve the herd thundering problem)
class acceptor : noncopyable {
 private:
  void create_new_fd(net::endpoint addr) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
      throw std::runtime_error("no resource available for socket allocation");
    }
    listenfd_ = fd;
    if (::bind(listenfd_, addr.as_sockaddr(), net::endpoint::len) < 0) {
      throw std::runtime_error("cannot bind port or sth wrong with the socket fd");
    }
  }

 public:
  typedef std::unique_ptr<tcp::connection> connection_ptr;
  /// Create a acceptor by rvalue parameters.
  /// \param ip  use rvalue for convenience, or just call ctor that accept an endpoint as parameter.
  /// \param port the port to listen
  /// \param backlog see man listen(2)
  explicit acceptor(std::string &&ip, uint16_t port, int backlog = 1024) : local_addr_{ip, port}, backlog_{backlog} {
    create_new_fd(local_addr_);
  }
  explicit acceptor(io_context *ctx, net::endpoint addr, int backlog = 1024) : local_addr_{addr}, backlog_{backlog} {
    create_new_fd(local_addr_);
  }
  explicit acceptor(io_context *context, std::string &&ip, uint16_t port, int max_connection = 65536,
                    int backlog = 1024)
      : local_addr_{ip, port}, backlog_{backlog} {
    create_new_fd(local_addr_);
  }
  explicit acceptor(net::endpoint addr, int backlog = 1024) : backlog_{backlog} { create_new_fd(local_addr_); }

 public:
  /// normally we won't read or write to a listen fd, just mark it with explicit
  /// \return a file_descriptor view
  operator file_descriptor() { return {listenfd_}; }

  int fd() { return listenfd_; }

  auto get_local_endpoint() { return local_addr_; }

  void set_nonblock() { ::fcntl(listenfd_, F_SETFL, O_NONBLOCK); }

  void enable() { ::listen(listenfd_, backlog_); }

  async_task<void> better_enable() {
    auto &ctx = coro::get_io_context_ref();
    ::listen(listenfd_, backlog_);
    backupfd_ = co_await ctx.openat(AT_FDCWD, "/dev/null", 0, O_RDONLY | O_CLOEXEC);
  }

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
      } else if (errno == ENFILE) {
        ::close(backupfd_);
        connfd = ::accept(listenfd_, peer_addr.as_sockaddr(), &addr_len);
        if (connfd > 0) {
          ::close(connfd);
          backupfd_ = ::openat(AT_FDCWD, "/dev/null", 0, O_RDONLY | O_CLOEXEC);
        }
      } else if (errno != EINTR) {
        throw std::system_error(std::error_code{-connfd, std::system_category()});
      }
    }
    return CONNECTION_TYPE{socket{connfd}, local_addr_, peer_addr};
  }

  template <typename CONNECTION_TYPE = tcp::connection>
  requires(std::is_same_v<tcp::connection, CONNECTION_TYPE> ||
           std::is_same_v<tcp::peer_connection, CONNECTION_TYPE>) async_task<CONNECTION_TYPE> accept() {
    auto &ctx = coro::get_io_context_ref();
    net::endpoint peer_addr{};
    auto addr_len = net::endpoint::len;
    auto connfd = co_await ctx.accept(listenfd_, peer_addr.as_sockaddr(), &addr_len);
    if (connfd == -ENFILE) {
      co_await ctx.close(backupfd_);
      connfd = co_await ctx.accept(listenfd_, peer_addr.as_sockaddr(), &addr_len);
      if (connfd > 0) {
        co_await ctx.close(connfd);
        backupfd_ = co_await ctx.openat(AT_FDCWD, "/dev/null", 0, O_RDONLY | O_CLOEXEC);
      }
    }
    if (connfd < 0 && connfd != -EINTR) {
      throw std::system_error(std::error_code{-connfd, std::system_category()});
    }
    co_return CONNECTION_TYPE(socket{connfd}, peer_addr);
  }

  void stop() {}

  ~acceptor() { ::close(listenfd_); }

 private:
  net::endpoint local_addr_;
  int backlog_;
  int listenfd_;
  int backupfd_;
};
}  // namespace coring::tcp

#endif  // CORING_ACCEPTOR_HPP
