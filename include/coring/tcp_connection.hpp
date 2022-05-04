/// TODO: A bad abstraction, remove it later
#ifndef CORING_TCP_CONNECTION_HPP
#define CORING_TCP_CONNECTION_HPP

#include "socket.hpp"
#include "coring/detail/time_utils.hpp"
namespace coring::detail {
/// templated function wraparound.
/// I cannot make it static
struct _tcp_connection_helper {
  /// make sure you pass a code >= 0, and make sure it's returned by connect syscall
  /// \param ret_code
  static inline void handle_connect_error(int ret_code) {
    if (ret_code == 0) return;
    if (ret_code == ECANCELED) {
      throw std::runtime_error("try connect to peer timeout, canceled");
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
      throw std::runtime_error("try connect to peer timeout, canceled");
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

inline auto make_socket() { return socket(new_socket_safe()); }

struct empty {
  explicit empty(const net::endpoint &) {
    // use const reference to reduce argument passing, since endpoint is not simple POD, and
    // passing it by value would be slow.
  }
  explicit empty(const socket &) {}
  empty(const net::endpoint &l, const net::endpoint &) {}
};

struct peer_endpoint {
  net::endpoint peer;
  explicit peer_endpoint(const net::endpoint &p) : peer{p} {}
  explicit peer_endpoint(const socket &s) : peer{s.peer_endpoint()} {}
  peer_endpoint(const net::endpoint &, const net::endpoint &p) : peer{p} {}
};

struct local_endpoint {
  net::endpoint local;
  explicit local_endpoint(const net::endpoint &l) : local{l} {}
  explicit local_endpoint(const socket &s) : local{s.local_endpoint()} {}
  local_endpoint(const net::endpoint &l, const net::endpoint &) : local{l} {}
};

struct socket_endpoints : local_endpoint, peer_endpoint {
  socket_endpoints(const net::endpoint &l, const net::endpoint &p) : local_endpoint{l}, peer_endpoint{p} {}
};

/// This connection cann be shared or copied.
/// It doesn't manage the socket fd it have,
/// when the object destroyed, the fd would be shutdown
/// (a.k.a. no FIN or RST is sent automatically)
/// and the file descriptor would not be closed.
/// \tparam AddrOption do you need a address as member of the socket. It consumes memory.
template <typename AddrOption>
class connection_base : public socket, public AddrOption {
 private:
 public:
  explicit connection_base(int fd) : socket{fd}, AddrOption{fd_} {}
  explicit connection_base(socket &&so) : socket{std::move(so)}, AddrOption{fd_} {}
  connection_base(socket &&so, const net::endpoint &local, const net::endpoint &peer)
      : socket{std::move(so)}, AddrOption(local, peer) {}
  connection_base(socket &&so, const net::endpoint &end) : socket{std::move(so)}, AddrOption(end) {}
  connection_base(connection_base &&rhs) : socket(std::move(rhs)), AddrOption(std::move(rhs)) {}
  ~connection_base() override = default;

  template <typename ToOtherAddr>
  requires std::is_same_v<AddrOption, empty> &&(!std::is_same_v<ToOtherAddr, empty>)
  operator connection_base<ToOtherAddr>() {  // NOLINT
    return connection_base<ToOtherAddr>{fd_};
  }

  /// I think socket would be a class provides low-level interfaces,
  /// so no exception are thrown here...
  /// People may have different argument on whether throw or not in EOF case
  /// \param dst
  /// \param nbytes expected count, short read may occurs
  /// \return
  inline detail::io_awaitable recv_some(char *dst, size_t nbytes, uint32_t fl = 0) {
    return coro::get_io_context_ref().recv(fd_, (void *)dst, (unsigned)nbytes, fl);
  }

  /// try read some and wait for `dur` timeout at most, this could be useful to impl keepalive
  /// application like HTTP etc.
  /// TODO: we can keep a constant kernal_timespec (relative) pool, to avoid coroutine overhead.
  /// FIXME: the recv_some(Dur) is lazy when recv_some() is eager...
  /// \tparam Duration a std::chrono duration type
  /// \param dst
  /// \param nbytes expected count, short read may occurs
  /// \param dur a relative timeout
  /// \return
  template <typename Duration>
  task<int> recv_some(char *dst, size_t nbytes, Duration &&dur, uint32_t fl = 0) {
    auto read_awaitable = coro::get_io_context_ref().recv(fd_, (void *)dst, (unsigned)nbytes, fl, IOSQE_IO_LINK);
    auto k = make_timespec(std::forward<Duration>(dur));
    coro::get_io_context_ref().link_timeout(&k);
    co_return co_await read_awaitable;
  }

  inline detail::io_awaitable read_some(char *dst, size_t nbytes) {
    return coro::get_io_context_ref().read(fd_, (void *)dst, (unsigned)nbytes, 0);
  }

  inline detail::io_awaitable send_some(char *dst, size_t nbytes, uint32_t fl = 0) {
    return coro::get_io_context_ref().send(fd_, (void *)dst, (unsigned)nbytes, fl);
  }

  template <typename Duration>
  task<int> send_some(char *dst, size_t nbytes, Duration &&dur, uint32_t fl = 0) {
    auto read_awaitable = coro::get_io_context_ref().send(fd_, (void *)dst, (unsigned)nbytes, fl, IOSQE_IO_LINK);
    auto k = make_timespec(std::forward<Duration>(dur));
    coro::get_io_context_ref().link_timeout(&k);
    co_return co_await read_awaitable;
  }

  inline detail::io_awaitable write_some(char *dst, size_t nbytes) {
    return coro::get_io_context_ref().write(fd_, (void *)dst, (unsigned)nbytes, 0);
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

  void set_keep_alive(bool on) {
    int optval = on ? 1 : 0;
    int ret = ::setsockopt(fd_, SOL_SOCKET, SO_KEEPALIVE, &optval, static_cast<socklen_t>(sizeof optval));
    if (ret < 0 && on) {
      // TODO: add a LOG_FATAL
      throw std::system_error(std::error_code{errno, std::system_category()});
    }
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
/// \return if you want to get a shared_ptr instead, just move it.
/// ctor 13 at: https://en.cppreference.com/w/cpp/memory/shared_ptr/shared_ptr
template <typename CONN_TYPE = connection>
task<CONN_TYPE> connect_to(const net::endpoint &peer) {
  int fd = tcp::new_socket_safe();
  int ret = co_await coro::get_io_context_ref().connect(fd, peer.as_sockaddr(), net::endpoint::len);
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
///  \return if you want to get a shared_ptr instead, just move it.
template <typename CONN_TYPE = connection, typename Duration>
requires(!std::is_same_v<std::remove_cvref<Duration>, net::endpoint>) task<CONN_TYPE> connect_to(
    const net::endpoint &peer, Duration &&dur) {
  int fd = tcp::new_socket_safe();
  auto connd_awaitable = coro::get_io_context_ref().connect(fd, peer.as_sockaddr(), net::endpoint::len, IOSQE_IO_LINK);
  auto k = make_timespec(std::forward<Duration>(dur));
  coro::get_io_context_ref().link_timeout(&k);
  int ret = co_await connd_awaitable;
  detail::_tcp_connection_helper::handle_connect_error(-ret, fd);
  co_return CONN_TYPE(fd);
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
/// \return if you want to get a shared_ptr instead, just move it.
template <typename CONN_TYPE = connection, typename Duration>
task<CONN_TYPE> connect_to(const net::endpoint &local, const net::endpoint &peer, Duration &&dur) {
  int fd = tcp::new_socket_safe();
  safe_bind_socket(fd, local.as_sockaddr());
  auto connd_awaitable = coro::get_io_context_ref().connect(fd, peer.as_sockaddr(), net::endpoint::len, IOSQE_IO_LINK);
  auto k = make_timespec(std::forward<Duration>(dur));
  coro::get_io_context_ref().link_timeout(&k);
  int ret = co_await connd_awaitable;
  detail::_tcp_connection_helper::handle_connect_error(-ret, fd);
  co_return CONN_TYPE(fd, local, peer);
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
  auto connd_awaitable = coro::get_io_context_ref().connect(fd, peer.as_sockaddr(), net::endpoint::len, IOSQE_IO_LINK);
  auto k = make_timespec(std::forward<Duration>(dur));
  coro::get_io_context_ref().link_timeout(&k);
  int ret = co_await connd_awaitable;
  detail::_tcp_connection_helper::handle_connect_error(-ret);
  co_return CONN_TYPE(fd);
}
}  // namespace coring::tcp

#endif  // CORING_TCP_CONNECTION_HPP
