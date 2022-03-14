
#ifndef CORING_SOCKET_HPP
#define CORING_SOCKET_HPP
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/tcp.h>

#include "coring/utils/buffer.hpp"
#include "endpoint.hpp"
namespace coring {
class socket {
 public:
  socket() {}

  int error() {
    int optval;
    socklen_t optlen = static_cast<socklen_t>(sizeof optval);
    if (::getsockopt(fd_, SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0) {
      return errno;
    } else {
      return optval;
    }
  }

  net::endpoint local_endpoint() {
    net::endpoint ep{};
    socklen_t addrlen = net::endpoint::len;
    if (::getsockname(fd_, ep.as_sockaddr(), &addrlen) < 0) {
      // TODO:  error handling, add a LOG_FATAL
      // use error_code
      throw std::runtime_error("bad socket local");
    }
    return ep;
  }
  net::endpoint peer_endpoint() {
    net::endpoint ep{};
    socklen_t addrlen = net::endpoint::len;
    if (::getpeername(fd_, ep.as_sockaddr(), &addrlen) < 0) {
      // TODO:  error handling, add a LOG_FATAL
      // use error_code
      throw std::runtime_error("bad socket peer");
    }
    return ep;
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
      // TODO:  error handling, add a LOG_FATAL
      throw std::system_error(std::error_code{errno, std::system_category()});
    }
  }

  void shutdown(int how) {
    if (::shutdown(fd_, how) < 0) {
      // TODO:  error handling, add a LOG_FATAL
      throw std::system_error(std::error_code{errno, std::system_category()});
    }
  }

  void shutdown() { shutdown(SHUT_RDWR); }
  void shutdown_write() { shutdown(SHUT_WR); }
  void shutdown_read() { shutdown(SHUT_RD); }

  void set_tcp_no_delay(bool on) {
    int optval = on ? 1 : 0;
    int ret = ::setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, &optval, static_cast<socklen_t>(sizeof optval));
    if (ret < 0 && on) {
      // TODO:  error handling, add a LOG_FATAL
      throw std::system_error(std::error_code{errno, std::system_category()});
    }
  }

  void set_reuse_addr(bool on) {
    int optval = on ? 1 : 0;
    int ret = ::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &optval, static_cast<socklen_t>(sizeof optval));
    if (ret < 0 && on) {
      // TODO:  error handling, add a LOG_FATAL
      throw std::system_error(std::error_code{errno, std::system_category()});
    }
  }

  void set_reuse_port(bool on) {
    int optval = on ? 1 : 0;
    int ret = ::setsockopt(fd_, SOL_SOCKET, SO_REUSEPORT, &optval, static_cast<socklen_t>(sizeof optval));
    if (ret < 0 && on) {
      // TODO:  error handling, add a LOG_FATAL
      throw std::system_error(std::error_code{errno, std::system_category()});
    }
  }

  void set_keep_alive(bool on) {
    int optval = on ? 1 : 0;
    int ret = ::setsockopt(fd_, SOL_SOCKET, SO_KEEPALIVE, &optval, static_cast<socklen_t>(sizeof optval));
    if (ret < 0 && on) {
      // TODO:  error handling, add a LOG_FATAL
      throw std::system_error(std::error_code{errno, std::system_category()});
    }
  }

 private:
  int fd_;
};
}  // namespace coring

#endif  // CORING_SOCKET_HPP
