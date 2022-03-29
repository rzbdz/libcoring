#include "socket.hpp"
int coring::socket::error() {
  int optval;
  socklen_t optlen = static_cast<socklen_t>(sizeof optval);
  if (::getsockopt(fd_, SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0) {
    return errno;
  } else {
    return optval;
  }
}

coring::net::endpoint coring::socket::local_endpoint() {
  net::endpoint ep{};
  socklen_t addrlen = net::endpoint::len;
  if (::getsockname(fd_, ep.as_sockaddr(), &addrlen) < 0) {
    // TODO:  error handling, add a LOG_FATAL
    // use error_code
    throw std::runtime_error("bad socket local");
  }
  return ep;
}
coring::net::endpoint coring::socket::peer_endpoint() {
  net::endpoint ep{};
  socklen_t addrlen = net::endpoint::len;
  if (::getpeername(fd_, ep.as_sockaddr(), &addrlen) < 0) {
    // TODO:  error handling, add a LOG_FATAL
    // use error_code
    throw std::runtime_error("bad socket peer");
  }
  return ep;
}
bool coring::socket::is_self_connect() {
  auto local = local_endpoint();
  auto peer = peer_endpoint();
  if (local.family() == AF_INET) {
    return local.port() == peer.port() &&
           local.as_sockaddr_in()->sin_addr.s_addr == peer.as_sockaddr_in()->sin_addr.s_addr;
  } else {
    return false;
  }
}
void coring::socket::setsockopt(int level, int optname, const void *optval, socklen_t optlen) {
  if (::setsockopt(fd_, level, optname, optval, optlen) < 0) {
    // I just know that the errno could use error_code
    // TODO:  error handling, add a LOG_FATAL
    throw std::system_error(std::error_code{errno, std::system_category()});
  }
}
void coring::socket::shutdown(int how) {
  if (::shutdown(fd_, how) < 0) {
    // TODO:  error handling, add a LOG_FATAL
    // We don't care, but it will be closed finally.
    // The point is that when the client shut it, we lose the game.
    // throw std::system_error(std::error_code{errno, std::system_category()});
    // std::cout << "Sad story the fd is not well-formed: " << fd_ << std::endl;
  }
}
void coring::socket::set_tcp_no_delay(bool on) {
  int optval = on ? 1 : 0;
  int ret = ::setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, &optval, static_cast<socklen_t>(sizeof optval));
  if (ret < 0 && on) {
    // TODO:  error handling, add a LOG_FATAL
    throw std::system_error(std::error_code{errno, std::system_category()});
  }
}
void coring::socket::set_reuse_addr(bool on) {
  int optval = on ? 1 : 0;
  int ret = ::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &optval, static_cast<socklen_t>(sizeof optval));
  if (ret < 0 && on) {
    // TODO:  error handling, add a LOG_FATAL
    throw std::system_error(std::error_code{errno, std::system_category()});
  }
}
void coring::socket::set_reuse_port(bool on) {
  int optval = on ? 1 : 0;
  int ret = ::setsockopt(fd_, SOL_SOCKET, SO_REUSEPORT, &optval, static_cast<socklen_t>(sizeof optval));
  if (ret < 0 && on) {
    // TODO:  error handling, add a LOG_FATAL
    throw std::system_error(std::error_code{errno, std::system_category()});
  }
}
void coring::socket::set_keep_alive(bool on) {
  int optval = on ? 1 : 0;
  int ret = ::setsockopt(fd_, SOL_SOCKET, SO_KEEPALIVE, &optval, static_cast<socklen_t>(sizeof optval));
  if (ret < 0 && on) {
    // TODO:  error handling, add a LOG_FATAL
    throw std::system_error(std::error_code{errno, std::system_category()});
  }
}
