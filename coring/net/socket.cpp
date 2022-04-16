#include "socket.hpp"
using namespace coring;
int socket::error() {
  int optval;
  socklen_t optlen = static_cast<socklen_t>(sizeof optval);
  if (::getsockopt(fd_, SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0) {
    return errno;
  } else {
    return optval;
  }
}

net::endpoint socket::local_endpoint() {
  net::endpoint ep{};
  socklen_t addrlen = net::endpoint::len;
  if (::getsockname(fd_, ep.as_sockaddr(), &addrlen) < 0) {
    // TODO: add a LOG_FATAL
    // use error_code
    throw std::runtime_error("bad socket local");
  }
  return ep;
}
net::endpoint socket::peer_endpoint() {
  net::endpoint ep{};
  socklen_t addrlen = net::endpoint::len;
  if (::getpeername(fd_, ep.as_sockaddr(), &addrlen) < 0) {
    // TODO: add a LOG_FATAL
    // use error_code
    throw std::runtime_error("bad socket peer");
  }
  return ep;
}
bool socket::is_self_connect() {
  auto local = local_endpoint();
  auto peer = peer_endpoint();
  if (local.family() == AF_INET) {
    return local.port() == peer.port() &&
           local.as_sockaddr_in()->sin_addr.s_addr == peer.as_sockaddr_in()->sin_addr.s_addr;
  } else {
    return false;
  }
}
void socket::setsockopt(int level, int optname, const void *optval, socklen_t optlen) {
  if (::setsockopt(fd_, level, optname, optval, optlen) < 0) {
    // I just know that the errno could use error_code
    // TODO: add a LOG_FATAL
    throw std::system_error(std::error_code{errno, std::system_category()});
  }
}

detail::io_awaitable socket::shutdown(int how) { return coro::get_io_context().shutdown(fd_, how); }

void socket::set_tcp_no_delay(bool on) {
  int optval = on ? 1 : 0;
  int ret = ::setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, &optval, static_cast<socklen_t>(sizeof optval));
  if (ret < 0 && on) {
    // TODO: add a LOG_FATAL
    throw std::system_error(std::error_code{errno, std::system_category()});
  }
}
void socket::set_reuse_addr(bool on) {
  int optval = on ? 1 : 0;
  int ret = ::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &optval, static_cast<socklen_t>(sizeof optval));
  if (ret < 0 && on) {
    // TODO: add a LOG_FATAL
    throw std::system_error(std::error_code{errno, std::system_category()});
  }
}
void socket::set_reuse_port(bool on) {
  int optval = on ? 1 : 0;
  int ret = ::setsockopt(fd_, SOL_SOCKET, SO_REUSEPORT, &optval, static_cast<socklen_t>(sizeof optval));
  if (ret < 0 && on) {
    // TODO: add a LOG_FATAL
    throw std::system_error(std::error_code{errno, std::system_category()});
  }
}
void socket::set_keep_alive(bool on) {
  int optval = on ? 1 : 0;
  int ret = ::setsockopt(fd_, SOL_SOCKET, SO_KEEPALIVE, &optval, static_cast<socklen_t>(sizeof optval));
  if (ret < 0 && on) {
    // TODO: add a LOG_FATAL
    throw std::system_error(std::error_code{errno, std::system_category()});
  }
}
int new_udp_socket_safe() {
  int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) {
    throw std::runtime_error("no resource available for socket allocation");
  }
  return fd;
}
void safe_bind_socket(int fd, const sockaddr *addr) {
  if (::bind(fd, addr, net::endpoint::len) < 0) {
    throw std::system_error(std::error_code{errno, std::system_category()});
  }
}
