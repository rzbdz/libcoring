
#ifndef CORING_SOCKET_HPP
#define CORING_SOCKET_HPP
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <utility>

#include "coring/buffer.hpp"
#include "endpoint.hpp"
#include "file_descriptor.hpp"
namespace coring {
// TODO: it's apparent that io_uring should support async socket syscalls
// but it's not done yet, https://github.com/axboe/liburing/issues/234
// mostly 5.19 from the post... There is still a long way to go...
class socket : public file_descriptor {
 protected:
 public:
  socket(int fd = -1) : file_descriptor{fd} {}  // supports for placeholder
  socket(socket &&so) : file_descriptor{std::move(so)} {}

  int fd() const { return fd_; }

  int error() const {
    int optval;
    socklen_t optlen = static_cast<socklen_t>(sizeof optval);
    if (::getsockopt(fd_, SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0) {
      return errno;
    } else {
      return optval;
    }
  }

  /// Interesting query
  [[nodiscard]] std::error_code error_code() const { return {error(), std::system_category()}; }

  [[nodiscard]] net::endpoint local_endpoint() const {
    net::endpoint ep{};
    socklen_t addrlen = net::endpoint::len;
    if (::getsockname(fd_, ep.as_sockaddr(), &addrlen) < 0) {
      // TODO: add a LOG_FATAL
      throw std::system_error(std::error_code{errno, std::system_category()});
    }
    return ep;
  }

  [[nodiscard]] net::endpoint peer_endpoint() const {
    net::endpoint ep{};
    socklen_t addrlen = net::endpoint::len;
    if (::getpeername(fd_, ep.as_sockaddr(), &addrlen) < 0) {
      // TODO: add a LOG_FATAL
      throw std::system_error(std::error_code{errno, std::system_category()});
    }
    return ep;
  }

  [[nodiscard]] bool is_self_connect() const {
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

  [[nodiscard]] detail::io_awaitable shutdown(int how) { return coro::get_io_context_ref().shutdown(fd_, how); }

  /// It would be better to use close instead of shutdown if the file descriptor has unique ownership
  /// in some kernel (tested in 5.17), shutdown would cause async punt, when close won't.
  /// \return
  [[nodiscard]] detail::io_awaitable shutdown() { return shutdown(SHUT_RDWR); }
  [[nodiscard]] detail::io_awaitable shutdown_write() { return shutdown(SHUT_WR); }
  [[nodiscard]] detail::io_awaitable shutdown_read() { return shutdown(SHUT_RD); }

  void set_reuse_port(bool on) {
    int optval = on ? 1 : 0;
    int ret = ::setsockopt(fd_, SOL_SOCKET, SO_REUSEPORT, &optval, static_cast<socklen_t>(sizeof optval));
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
inline auto make_udp_socket() {
  int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) {
    throw std::runtime_error("no resource available for socket allocation");
  }
  return socket(fd);  // it would be moved
}
inline auto make_shared_udp_socket() {
  int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) {
    throw std::runtime_error("no resource available for socket allocation");
  }
  return std::make_shared<socket>(fd);
}
}  // namespace coring

#endif  // CORING_SOCKET_HPP
