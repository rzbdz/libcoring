
#ifndef CORING_SOCKET_HPP
#define CORING_SOCKET_HPP
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/tcp.h>

#include "coring/utils/buffer.hpp"
#include "endpoint.hpp"
#include "file_descriptor.hpp"
namespace coring {
class socket : public file_descriptor {
 protected:
 public:
  socket(int fd = -1) : file_descriptor{fd} {}
  int fd() { return fd_; }
  void set_fd(int fd) { fd_ = fd; }
  int error();
  std::error_code error_code() { return {error(), std::system_category()}; }
  net::endpoint local_endpoint();
  net::endpoint peer_endpoint();

  bool is_self_connect();
  void setsockopt(int level, int optname, const void *optval, socklen_t optlen);

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
  detail::io_awaitable shutdown(int how);

  detail::io_awaitable shutdown() { return shutdown(SHUT_RDWR); }
  detail::io_awaitable shutdown_write() { return shutdown(SHUT_WR); }
  detail::io_awaitable shutdown_read() { return shutdown(SHUT_RD); }

  void set_tcp_no_delay(bool on);

  void set_reuse_addr(bool on);

  void set_reuse_port(bool on);

  void set_keep_alive(bool on);
};
}  // namespace coring

#endif  // CORING_SOCKET_HPP
