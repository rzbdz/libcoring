/// TODO: A bad abstraction, remove it later
#ifndef CORING_TCP_CONNECTION_HPP
#define CORING_TCP_CONNECTION_HPP
#include "socket.hpp"
#include "coring/utils/time_utils.hpp"
namespace coring::detail {
/// templated function wraparound.
/// I cannot make it static
struct _tcp_connection_helper {
  /// make sure you pass a code >= 0, and make sure it's returned by connect syscall
  /// \param ret_code
  static inline void handle_connect_error(int ret_code) {
    if (ret_code == 0) return;
    if (ret_code == ECANCELED) {
      throw std::runtime_error("connect canceled");
    }
    throw std::system_error(std::error_code{ret_code, std::system_category()});
  }
  /// make sure you pass a code >= 0, and make sure it's returned by connect syscall
  /// \param ret_code
  static inline void handle_connect_error(int ret_code, int tempfd_to_close) {
    if (ret_code == 0) return;
    // it's ok to close directly since it's not connected, but this take a
    // syscall...
    // I think if we call ioc->close do submitted a sqe, just with no coroutine_handle is registered...
    // Just use close right now, when you meet an error, it's unavoidable...
    // TODO: close may be interrupted... -> EINTR
    // see: man 2 close NOTE, there is a discussion of close loop...
    // while((int ret = ::close(tempfd_to_close)) != 0);
    // Is it work?
    ::close(tempfd_to_close);
    if (ret_code == ECANCELED) {
      throw std::runtime_error("connect canceled");
    }
    throw std::system_error(std::error_code{ret_code, std::system_category()});
  }
};
}  // namespace coring::detail
namespace coring::tcp {
/// allocate a new tcp socket from system
/// throw if error.
/// \return
inline int new_socket_safe() {
  int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    throw std::runtime_error("no resource available for socket allocation");
  }
  return fd;
}

struct empty {
  explicit empty(net::endpoint) {}
  explicit empty(socket) {}
  empty(net::endpoint l, net::endpoint) {}
};
struct peer_endpoint {
  net::endpoint peer;
  explicit peer_endpoint(const net::endpoint &p) : peer{p} {}
  explicit peer_endpoint(socket s) : peer{s.peer_endpoint()} {}
  peer_endpoint(const net::endpoint &, const net::endpoint &p) : peer{p} {}
};
struct local_endpoint {
  net::endpoint local;
  explicit local_endpoint(const net::endpoint &l) : local{l} {}
  explicit local_endpoint(socket s) : local{s.local_endpoint()} {}
  local_endpoint(const net::endpoint &l, const net::endpoint &) : local{l} {}
};
struct socket_endpoints : local_endpoint, peer_endpoint {
  socket_endpoints(const net::endpoint &l, const net::endpoint &p) : local_endpoint{l}, peer_endpoint{p} {}
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
  connection_base(socket so, const net::endpoint &local, const net::endpoint &peer)
      : socket{so}, AddrOption(local, peer) {}
  connection_base(socket so, const net::endpoint &end) : socket{so}, AddrOption(end) {}
  ~connection_base() = default;

  template <typename ToOtherAddr>
  requires std::is_same_v<AddrOption, empty> &&(!std::is_same_v<ToOtherAddr, empty>)
  operator connection_base<ToOtherAddr>() {  // NOLINT
    return connection_base<ToOtherAddr>{fd_};
  }
};
typedef connection_base<empty> connection;
typedef connection_base<peer_endpoint> peer_connection;
typedef connection_base<local_endpoint> local_connection;
typedef connection_base<socket_endpoints> socket_connection;

/// It' s not safe for it create a socket fd implicitly,
/// If an exception is thrown, fd might be closed, might not...
/// Call to connect_to(fd, ...) is strongly recommended, but
/// the user need to create a socket by calling ::socket.../ or just tcp::new_socket_safe()
///  \tparam CONN_TYPE@code
/// connection;
/// peer_connection;
/// local_connection;
/// socket_connection; @endcode
/// \param peer
/// \return
template <typename CONN_TYPE = connection>
task<CONN_TYPE> connect_to(const net::endpoint &peer) {
  int fd = tcp::new_socket_safe();
  // std::cout << "go co_await connect" << std::endl;
  int ret = co_await coro::get_io_context().connect(fd, peer.as_sockaddr(), net::endpoint::len, 0);
  detail::_tcp_connection_helper::handle_connect_error(-ret, fd);
  co_return CONN_TYPE(fd);
}

template <typename CONN_TYPE = connection>
async_task<CONN_TYPE> connect_to(const net::endpoint &peer, io_cancel_token token) {
  int fd = tcp::new_socket_safe();
  int ret = co_await coro::get_io_context().connect(fd, peer.as_sockaddr(), net::endpoint::len, 0, token);
  detail::_tcp_connection_helper::handle_connect_error(-ret, fd);
  co_return CONN_TYPE(fd);
}

/// It' s not safe for it create a socket fd implicitly,
/// If an exception is thrown, fd might be closed, might not...
/// Call to connect_to(fd, ...) is strongly recommended, but
/// the user need to create a socket by calling ::socket.../ or just tcp::new_socket_safe()
/// \tparam CONN_TYPE@code
/// connection;
/// peer_connection;
/// local_connection;
/// socket_connection; @endcode
///  \tparam Duration just using std::chrono_literals
///  \param peer
///  \param dur a time(relative duration)
///  \return
template <typename CONN_TYPE = connection, typename Duration>
requires(!std::is_same_v<std::remove_cvref<Duration>, net::endpoint>) task<CONN_TYPE> connect_to(
    const net::endpoint &peer, Duration &&dur) {
  int fd = tcp::new_socket_safe();
  auto connd_awaitable = coro::get_io_context().connect(fd, peer.as_sockaddr(), net::endpoint::len, IOSQE_IO_LINK);
  auto k = make_timespec(std::forward<Duration>(dur));
  coro::get_io_context().link_timeout(&k);
  int ret = co_await connd_awaitable;
  detail::_tcp_connection_helper::handle_connect_error(-ret, fd);
  co_return CONN_TYPE{fd};
}
/// It' s not safe for it create a socket fd implicitly,
/// If an exception is thrown, fd might be closed, might not...
/// Call to connect_to(fd, ...) is strongly recommended, but
/// the user need to create a socket by calling ::socket.../ or just tcp::new_socket_safe()
///  \tparam CONN_TYPE@code
/// connection;
/// peer_connection;
/// local_connection;
/// socket_connection; @endcode
/// \tparam Duration just using std::chrono_literals
/// \param local ::bind is performed on it
/// \param peer
/// \param dur a time(relative duration)
/// \return
template <typename CONN_TYPE = connection, typename Duration>
task<CONN_TYPE> connect_to(const net::endpoint &local, const net::endpoint &peer, Duration &&dur) {
  int fd = tcp::new_socket_safe();
  safe_bind_socket(fd, local.as_sockaddr());
  auto connd_awaitable = coro::get_io_context().connect(fd, peer.as_sockaddr(), net::endpoint::len, IOSQE_IO_LINK);
  auto k = make_timespec(std::forward<Duration>(dur));
  coro::get_io_context().link_timeout(&k);
  int ret = co_await connd_awaitable;
  detail::_tcp_connection_helper::handle_connect_error(-ret, fd);
  co_return CONN_TYPE{fd, local, peer};
}
/// The user need to create a socket by calling ::socket.../ or just tcp::new_socket_safe()
/// this make sure the fd is maintained by caller, which can remake or freed even when
/// exceptions occurs.
///  \tparam CONN_TYPE@code
/// connection;
/// peer_connection;
/// local_connection;
/// socket_connection; @endcode
/// \tparam Duration just using std::chrono_literals
/// \param fd a socket fd
/// \param peer the peer address, if you want to set the local address, just call to bind before this.
/// \param dur a time(relative duration)
/// \return
template <typename CONN_TYPE = connection, typename Duration>
task<CONN_TYPE> connect_to(int fd, const net::endpoint &peer, Duration &&dur) {
  auto connd_awaitable = coro::get_io_context().connect(fd, peer.as_sockaddr(), net::endpoint::len, IOSQE_IO_LINK);
  auto k = make_timespec(std::forward<Duration>(dur));
  coro::get_io_context().link_timeout(&k);
  int ret = co_await connd_awaitable;
  detail::_tcp_connection_helper::handle_connect_error(-ret);
  co_return CONN_TYPE{fd};
}
}  // namespace coring::tcp

#endif  // CORING_TCP_CONNECTION_HPP
