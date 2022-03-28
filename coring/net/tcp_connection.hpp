
#ifndef CORING_TCP_CONNECTION_HPP
#define CORING_TCP_CONNECTION_HPP

#include "socket.hpp"
namespace coring::tcp {
struct empty {
  explicit empty(net::endpoint) {}
  explicit empty(socket) {}
  empty(net::endpoint l, net::endpoint) {}
};
struct peer_endpoint {
  net::endpoint peer;
  explicit peer_endpoint(net::endpoint p) : peer{p} {}
  explicit peer_endpoint(socket s) : peer{s.peer_endpoint()} {}
};
struct local_endpoint {
  net::endpoint local;
  explicit local_endpoint(net::endpoint p) : local{p} {}
  explicit local_endpoint(socket s) : local{s.local_endpoint()} {}
};
struct socket_endpoints : local_endpoint, peer_endpoint {
  socket_endpoints(net::endpoint l, net::endpoint p) : local_endpoint{l}, peer_endpoint{p} {}
};
/// This connection cann be shared or copied.
/// It doesn't manage the socket fd it have,
/// when the object destroyed, the fd would not be shutdown
/// (a.k.a. no FIN or RST is sent automatically)
/// and the file descriptor would not be closed.
/// Basically for supporting shared_connection, say write to socket on a thread
/// and read from another thread.
/// If you need a RAII connection, just write one.
/// TODO: I will try to write a unique_connection and a shared_connection with RAII together with reference counter.
/// \tparam AddrOption do you need a address as member of the socket. It consumes memory.
template <typename AddrOption>
class connection_base : public socket, public AddrOption {
 private:
 public:
  explicit connection_base(int fd) : socket{fd}, AddrOption{fd_} {}
  explicit connection_base(socket so) : socket{so}, AddrOption{fd_} {}
  connection_base(socket so, net::endpoint local, net::endpoint peer) : socket{so}, AddrOption(local, peer) {}
  connection_base(socket so, net::endpoint end) : socket{so}, AddrOption(end) {}
  ~connection_base() = default;
};
typedef connection_base<empty> connection;
typedef connection_base<peer_endpoint> peer_connection;
typedef connection_base<local_endpoint> local_connection;
typedef connection_base<socket_endpoints> socket_connection;

}  // namespace coring::tcp

#endif  // CORING_TCP_CONNECTION_HPP
