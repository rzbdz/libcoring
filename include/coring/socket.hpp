
#ifndef CORING_SOCKET_HPP
#define CORING_SOCKET_HPP
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/tcp.h>

#include "coring/buffer.hpp"
#include "endpoint.hpp"
#include "file_descriptor.hpp"
namespace coring {
class socket : public file_descriptor {
 protected:
 public:
  socket(int fd = -1) : file_descriptor{fd} {}
  int fd() { return fd_; }
  void set_fd(int fd) { fd_ = fd; }
  int error() {
    int optval;
    socklen_t optlen = static_cast<socklen_t>(sizeof optval);
    if (::getsockopt(fd_, SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0) {
      return errno;
    } else {
      return optval;
    }
  }
  std::error_code error_code() { return {error(), std::system_category()}; }
  net::endpoint local_endpoint() {
    net::endpoint ep{};
    socklen_t addrlen = net::endpoint::len;
    if (::getsockname(fd_, ep.as_sockaddr(), &addrlen) < 0) {
      // TODO: add a LOG_FATAL
      // use error_code
      throw std::runtime_error("bad socket local");
    }
    return ep;
  }
  net::endpoint peer_endpoint() {
    net::endpoint ep{};
    socklen_t addrlen = net::endpoint::len;
    if (::getpeername(fd_, ep.as_sockaddr(), &addrlen) < 0) {
      // TODO: add a LOG_FATAL
      // use error_code
      throw std::runtime_error("bad socket peer");
    }
    return ep;
  }

  inline detail::io_awaitable read_some(char *dst, size_t nbytes) {
    return coro::get_io_context_ref().recv(fd_, (void *)dst, (unsigned)nbytes, 0);
  }

  template <typename Duration>
  detail::io_awaitable read_some(char *dst, size_t nbytes, Duration &&dur) {
    auto read_awaitable = coro::get_io_context_ref().recv(fd_, (void *)dst, (unsigned)nbytes, 0);
    auto k = make_timespec(std::forward<Duration>(dur));
    coro::get_io_context_ref().link_timeout(&k);
    return read_awaitable;
  }

  bool is_self_connect() {
    auto local = local_endpoint();
    auto peer = peer_endpoint();
    if (local.family() == AF_INET) {
      return local.port() == peer.port() &&
             local.as_sockaddr_in()->sin_addr.s_addr == peer.as_sockaddr_in()->sin_addr.s_addr;
    } else {
      return false;
    }
  }
  void setsockopt(int level, int optname, const void *optval, socklen_t optlen) {
    if (::setsockopt(fd_, level, optname, optval, optlen) < 0) {
      // I just know that the errno could use error_code
      // TODO: add a LOG_FATAL
      throw std::system_error(std::error_code{errno, std::system_category()});
    }
  }

  /// set the linger option
  /// If your process call close(), FIN would be sent from the closing side by default
  /// note: you can call set_linger to set socket option SO_LINGER to make it send RST instead of FIN
  /// If your process exit without closing the socket, kernel would close the tcp connection and
  /// do the clean up for your process. FIN or RST can be sent.
  /// If there is data in your receive queue, RST would be sent. Otherwise, FIN would be sent.
  /// Go through tcp_close() in tcp.c for more details.
  ///------------------------------------------------------------------------------------------------------------
  /// <p>--- onoff = 0 => close won't block, RST sent if recv queue is not empty (which means you didn't handle all
  /// request, bad story), FIN sent if recv queue is empty, a good ending.</p>
  /// <p>--- linger = 0 => send RST immediately. </p>
  /// <p>--- linger !=0 => try to do case onoff = 0 if no timeout(linger seconds replaces tcp_fin_timeout) </p>
  /// If enable this use default parameter, when you call close, it would block.
  /// you should use io_context::close to close the socket.
  void set_linger(u_short onoff = 1, ushort linger = 30) {
    struct linger tmp = {onoff, linger};
    setsockopt(SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
  }
  [[nodiscard]] detail::io_awaitable shutdown(int how) { return coro::get_io_context_ref().shutdown(fd_, how); }

  [[nodiscard]] detail::io_awaitable shutdown() { return shutdown(SHUT_RDWR); }
  [[nodiscard]] detail::io_awaitable shutdown_write() { return shutdown(SHUT_WR); }
  [[nodiscard]] detail::io_awaitable shutdown_read() { return shutdown(SHUT_RD); }

  void set_tcp_no_delay(bool on) {
    int optval = on ? 1 : 0;
    int ret = ::setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, &optval, static_cast<socklen_t>(sizeof optval));
    if (ret < 0 && on) {
      // TODO: add a LOG_FATAL
      throw std::system_error(std::error_code{errno, std::system_category()});
    }
  }

  void set_reuse_addr(bool on) {
    int optval = on ? 1 : 0;
    int ret = ::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &optval, static_cast<socklen_t>(sizeof optval));
    if (ret < 0 && on) {
      // TODO: add a LOG_FATAL
      throw std::system_error(std::error_code{errno, std::system_category()});
    }
  }

  void set_reuse_port(bool on) {
    int optval = on ? 1 : 0;
    int ret = ::setsockopt(fd_, SOL_SOCKET, SO_REUSEPORT, &optval, static_cast<socklen_t>(sizeof optval));
    if (ret < 0 && on) {
      // TODO: add a LOG_FATAL
      throw std::system_error(std::error_code{errno, std::system_category()});
    }
  }
  void set_keep_alive(bool on) {
    int optval = on ? 1 : 0;
    int ret = ::setsockopt(fd_, SOL_SOCKET, SO_KEEPALIVE, &optval, static_cast<socklen_t>(sizeof optval));
    if (ret < 0 && on) {
      // TODO: add a LOG_FATAL
      throw std::system_error(std::error_code{errno, std::system_category()});
    }
  }
};
/// allocate a new udp socket from system
/// throw if error.
/// \return
inline int new_udp_socket_safe() {
  int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) {
    throw std::runtime_error("no resource available for socket allocation");
  }
  return fd;
}
/// bind a local address to a socket
/// \param fd
/// \param addr make sure its from a net::endpoint
inline void safe_bind_socket(int fd, const sockaddr *addr) {
  if (::bind(fd, addr, net::endpoint::len) < 0) {
    throw std::system_error(std::error_code{errno, std::system_category()});
  }
}
}  // namespace coring

#endif  // CORING_SOCKET_HPP
