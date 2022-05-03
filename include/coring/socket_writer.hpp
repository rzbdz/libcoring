
#ifndef CORING_SOCKET_WRITER_HPP
#define CORING_SOCKET_WRITER_HPP
#include <coroutine>
#include <cstddef>
#include "file_descriptor.hpp"
#include "coring/buffer.hpp"
#include "coring/task.hpp"
#include "coring/io_context.hpp"
#include "coring/buffer_pool.hpp"
#include "socket.hpp"
#include "eof_error.hpp"

namespace coring {
/// Write some bytes to `sock` from `buffer`, eof is not an error,
/// I use pointer as argument is doing it by convention, only use const references
/// and pointers.
/// \return how many bytes are written to the socket, 0 if eof
template <typename Buffer, typename TcpConnection>
inline async_task<int> write_some(TcpConnection *sock, Buffer *buffer) {
  auto n = co_await sock->send_some(buffer->front(), buffer->readable());
  if (n < 0 && n != -EINTR) {
    throw std::system_error(std::error_code{-n, std::system_category()});
  }
  buffer->has_read(n);
  co_return n;
}

/// Write some bytes to `sock` from `buffer`, eof is not an error
/// \return how many bytes are written to the socket, 0 if eof
template <typename Buffer, typename TcpConnection, class Dur>
inline async_task<int> write_some(TcpConnection *sock, Buffer *buffer, Dur &&dur) {
  auto n = co_await sock->send_some(buffer->front(), buffer->readable(), std::forward<Dur>(dur));
  if (n < 0 && n != -EINTR) {
    throw std::system_error(std::error_code{-n, std::system_category()});
  }
  buffer->has_read(n);
  co_return n;
}

/// Write all bytes from buffer to sock, eof is treated as an error (`coring::eof_error` is thrown)
template <typename Buffer, typename TcpConnection>
inline async_task<> write_certain(TcpConnection *sock, Buffer *buffer, int nbytes) {
  auto total = nbytes;
  while (total != 0) {
    auto n = co_await sock->send_some(buffer->front(), buffer->readable());
    if (n == 0) {
      throw coring::eof_error{};
    }
    if (n < 0 && n != -EINTR) {
      throw std::system_error(std::error_code{-n, std::system_category()});
    }
    buffer->has_read(n);
    total -= n;
  }
}

/// Write all bytes from buffer to sock, eof is treated as an error (`coring::eof_error` is thrown)
template <typename Buffer, typename TcpConnection, class Dur>
inline async_task<> write_certain(TcpConnection *sock, Buffer *buffer, int nbytes, Dur &&dur) {
  auto total = nbytes;
  while (total != 0) {
    auto n = co_await sock->send_some(buffer->front(), buffer->readable(), std::forward<Dur>(dur));
    if (n == 0) {
      throw coring::eof_error{};
    }
    if (n < 0 && n != -EINTR) {
      throw std::system_error(std::error_code{-n, std::system_category()});
    }
    buffer->has_read(n);
    total -= n;
  }
}

/// Write all bytes from buffer to sock, eof is treated as an error (`coring::eof_error` is thrown)
template <typename Buffer, typename TcpConnection>
inline async_task<> write_all(TcpConnection *sock, Buffer *buffer) {
  auto total = buffer->readable();
  while (total != 0) {
    auto n = co_await sock->send_some(buffer->front(), buffer->readable());
    if (n == 0) {
      throw coring::eof_error{};
    }
    if (n < 0 && n != -EINTR) {
      throw std::system_error(std::error_code{-n, std::system_category()});
    }
    buffer->has_read(n);
    total = buffer->readable();
  }
}

/// Write all bytes from buffer to sock, eof is treated as an error (`coring::eof_error` is thrown)
template <typename Buffer, typename TcpConnection, class Dur>
inline async_task<> write_all(TcpConnection *sock, Buffer *buffer, Dur &&dur) {
  auto total = buffer->readable();
  while (total != 0) {
    auto n = co_await sock->send_some(buffer->front(), buffer->readable(), std::forward<Dur>(dur));
    if (n == 0) {
      throw coring::eof_error{};
    }
    if (n < 0 && n != -EINTR) {
      throw std::system_error(std::error_code{-n, std::system_category()});
    }
    buffer->has_read(n);
    total = buffer->readable();
  }
}

}  // namespace coring

#endif  // CORING_SOCKET_WRITER_HPP
