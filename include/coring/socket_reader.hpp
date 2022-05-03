
#ifndef CORING_SOCKET_READER_HPP
#define CORING_SOCKET_READER_HPP
#include <coroutine>
#include <cstddef>
#include "coring/coring_config.hpp"
#include "file_descriptor.hpp"
#include "coring/buffer.hpp"
#include "coring/task.hpp"
#include "coring/io_context.hpp"
#include "coring/eof_error.hpp"
#include "socket.hpp"

namespace coring {

/// Read not certain bytes from sock, this treat EOF an error
/// since when user call this coroutine, they expect at least 1 byte received.
/// \param sock a socket
/// \param buffer a buffer
/// \return 0 if EOF, other, the bytes read from sock
template <typename Buffer, typename TcpConnection>
inline async_task<int> read_some(TcpConnection *sock, Buffer *buffer) {
  buffer->make_room(READ_BUFFER_AT_LEAST_WRITABLE);
  int ret = co_await sock->recv_some(buffer->back(), buffer->writable());
  // LOG_TRACE("recv_some returns: ", ret);
  if (ret == 0) {
    throw coring::eof_error{};
  }
  if (ret < 0) {
    throw std::system_error(std::error_code{-ret, std::system_category()});
  }
  buffer->has_written(ret);
  co_return ret;
}

/// Read not certain bytes from sock, this treat EOF an error
/// since when user call this coroutine, they expect at least 1 byte received.
/// \param sock a socket
/// \param buffer a buffer
/// \param dur a duration, relative, please use chrono_literals
/// \return the bytes read from sock, always positive
template <typename Buffer, typename TcpConnection, class Dur>
inline async_task<int> read_some(TcpConnection *sock, Buffer *buffer, Dur &&dur) {
  buffer->make_room(READ_BUFFER_AT_LEAST_WRITABLE);
  int ret = co_await sock->recv_some(buffer->back(), buffer->writable(), std::forward<Dur>(dur));
  // LOG_TRACE("recv_some returns: ", ret);
  if (ret == 0) {
    throw coring::eof_error{};
  }
  if (ret < 0) {
    throw std::system_error(std::error_code{-ret, std::system_category()});
  }
  buffer->has_written(ret);
  co_return ret;
}

/// read certain bytes, short-reads are guaranteed not occurs except for EOF
/// \param sock a tcp socket
/// \param buffer a buffer (fixed/flex)
/// \param nbytes certain bytes want to read into buffer, it not sufficient before EOF, error thrown
template <typename Buffer, typename TcpConnection>
inline async_task<> read_certain(TcpConnection *sock, Buffer *buffer, int nbytes) {
  while (buffer->readable() < nbytes) {
    int ret = co_await sock->recv_some(buffer->back(), buffer->writable());
    if (ret == 0) {
      throw coring::eof_error{};
    }
    if (ret < 0) {
      throw std::system_error(std::error_code{-ret, std::system_category()});
    }
  }
}

/// read certain bytes, short-reads are guaranteed not occurs except for EOF with timeout
/// \param sock a tcp socket
/// \param buffer a buffer (fixed/flex)
/// \param nbytes certain bytes want to read into buffer, it not sufficient before EOF, error thrown
/// \param dur a duration for every separate read compensate the short-count
template <typename Buffer, typename TcpConnection, class Dur>
inline async_task<> read_certain(TcpConnection *sock, Buffer *buffer, int nbytes, Dur &&dur) {
  while (buffer->readable() < nbytes) {
    int ret = co_await sock->recv_some(buffer->back(), buffer->writable(), std::forward<Dur>(dur));
    if (ret == 0) {
      throw coring::eof_error{};
    }
    if (ret < 0) {
      throw std::system_error(std::error_code{-ret, std::system_category()});
    }
  }
}

/// read until a line is in buffer
/// \return the length of this line, then you can create a `std::string_view`.
template <typename Buffer, typename TcpConnection>
async_task<int> read_line(TcpConnection *sock, Buffer *buffer) {
  const char *end = nullptr;
  if (buffer->readable() > 0) {
    end = buffer->find_eol();
  }
  while (end == nullptr) {
    int ret = co_await sock->recv_some(buffer->back(), buffer->writable());
    end = buffer->find_eol();
    if (ret == 0) {
      throw coring::eof_error{};
    }
    if (ret < 0) {
      throw std::system_error(std::error_code{-ret, std::system_category()});
    }
  }
  co_return end - buffer->front() + 1;  // the length
}

/// read until a lf line (\n) is in buffer
/// \param dur a duration for every separate read compensate the short-count.
/// \return the length of this line, then you can create a `std::string_view`.
template <typename Buffer, typename TcpConnection, typename Dur>
inline async_task<int> read_line(TcpConnection *sock, Buffer *buffer, Dur &&dur) {
  const char *end = nullptr;
  if (buffer->readable() > 0) {
    end = buffer->find_eol();
  }
  while (end == nullptr) {
    int ret = co_await sock->recv_some(buffer->back(), buffer->writable(), std::forward<Dur>(dur));
    end = buffer->find_eol();
    if (ret == 0) {
      throw coring::eof_error{};
    }
    if (ret < 0) {
      throw std::system_error(std::error_code{-ret, std::system_category()});
    }
  }
  co_return end - buffer->front() + 1;  // the length
}

/// read until a crlf line is in buffer
/// \return the length of this line, then you can create a `std::string_view`.
template <typename Buffer, typename TcpConnection>
inline async_task<int> read_crlf_line(TcpConnection *sock, Buffer *buffer) {
  const char *end = nullptr;
  if (buffer->readable() > 0) {
    end = buffer->find_crlf();
  }
  while (end == nullptr) {
    int ret = co_await sock->recv_some(buffer->back(), buffer->writable());
    end = buffer->find_crlf();
    if (ret == 0) {
      throw coring::eof_error{};
    }
    if (ret < 0) {
      throw std::system_error(std::error_code{-ret, std::system_category()});
    }
  }
  co_return end - buffer->front() + 2;  // the length
}

/// read until a crlf line (\r\n) is in buffer
/// \param dur a duration for every separate read compensate the short-count.
/// \return the length of this line, then you can create a `std::string_view`.
template <typename Buffer, typename TcpConnection, typename Dur>
inline async_task<int> read_crlf_line(TcpConnection *sock, Buffer *buffer, Dur &&dur) {
  const char *end = nullptr;
  if (buffer->readable() > 0) {
    end = buffer->find_crlf();
  }
  while (end == nullptr) {
    int ret = co_await sock->recv_some(buffer->back(), buffer->writable(), std::forward<Dur>(dur));
    end = buffer->find_crlf();
    if (ret == 0) {
      throw coring::eof_error{};
    }
    if (ret < 0) {
      throw std::system_error(std::error_code{-ret, std::system_category()});
    }
  }
  co_return end - buffer->front() + 1;  // the length
}

}  // namespace coring

#endif  // CORING_SOCKET_READER_HPP
