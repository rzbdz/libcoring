
#ifndef CORING_SOCKET_WRITER_HPP
#define CORING_SOCKET_WRITER_HPP
#include <coroutine>
#include <cstddef>
#include "file_descriptor.hpp"
#include "coring/utils/buffer.hpp"
#include "coring/async/task.hpp"
#include "coring/io/io_context.hpp"
#include "coring/net/buffer_pool.hpp"
#include "socket.hpp"
namespace coring {
/// \tparam UpperType the buffer type
template <typename UpperType = buffer>
class socket_writer_base {
 public:
  template <typename... Args>
  explicit socket_writer_base(socket fd, Args &&...args) : fd_{fd}, upper_layer_{std::forward<Args>(args)...} {
    static_assert(std::is_constructible_v<UpperType, Args...>);
  }

 private:
  void handle_write_error(int ret_code) {
    if (ret_code == 0) {
      throw std::runtime_error("socket may be closed, encounter EOF");
    }
    if (ret_code < 0 && ret_code != -EINTR) {
      throw std::system_error(std::error_code{-ret_code, std::system_category()});
    }
  }

 public:
  /// just one write.
  /// \param
  /// \return
  [[nodiscard]] task<int> write_to_file() {
    auto &ctx = coro::get_io_context_ref();
    // use send instead of read benefits from internal poll/epoll mechanism (maybe)
    auto n = co_await ctx.send(fd_, upper_layer_.front(), upper_layer_.readable(), 0);
    handle_write_error(n);
    upper_layer_.pop_front(n);
    co_return n;
  }
  /// write all to file, we must handle exception
  /// \param
  /// \return
  task<size_t> write_all_to_file() {
    size_t n = upper_layer_.readable();
    auto &ctx = coro::get_io_context_ref();
    while (n != 0) {
      // LOG_TRACE("co await send");
      auto writed = co_await ctx.send(fd_, upper_layer_.front(), upper_layer_.readable(), 0);
      LOG_TRACE("co await send: {} bytes", writed);
      handle_write_error(writed);
      upper_layer_.pop_front(writed);
      n -= writed;
    }
    // FIXME: useless return value...
    co_return n;
  }

  task<> write_certain_incrementally(const char *place, size_t nbytes) {
    static_assert(std::is_same_v<UpperType, buffer>);
    upper_layer_.emplace_back(place, nbytes);
    co_await write_to_file();
  }
  task<> write_certain_loosely(const char *place, size_t nbytes) {
    static_assert(std::is_same_v<UpperType, buffer>);
    upper_layer_.emplace_back(place, nbytes);
    if (upper_layer_.readable() >= 64 * 1024) {
      co_await write_to_file();
    }
  }

  task<> write_certain_strictly(const char *place, size_t nbytes) {
    upper_layer_.emplace_back(place, nbytes);
    co_await write_all_to_file();
  }

  task<> write_certain(const char *place, size_t nbytes) {
    upper_layer_.emplace_back(place, nbytes);
    co_await write_to_file();
  }
  UpperType &raw_buffer() { return upper_layer_; }

 private:
  socket fd_;
  UpperType upper_layer_;
};

/// wrap a socket with a buffer...
/// I don't know if there are other approach.
/// \return a buffered io, recommended using auto
template <typename... Args>
requires std::is_constructible_v<fixed_buffer, Args...>
inline auto socket_writer(socket so, Args &&...args) {
  return socket_writer_base<fixed_buffer>{so, std::forward<Args>(args)...};
}

/// wrap a socket with a buffer...
/// I don't know if there are other approach.
/// \return a buffered io, recommended using auto
template <typename... Args>
requires std::is_constructible_v<flex_buffer, Args...>
inline auto socket_writer(socket so, Args &&...args) {
  return socket_writer_base<flex_buffer>{so, std::forward<Args>(args)...};
}
/// TODO: bad abstraction
/// \param so
/// \param buffer
/// \return a stateful buffered
inline auto socket_writer(socket so, selected_buffer &buffer) {
  auto ret = socket_writer_base<fixed_buffer>{so, buffer.data(), buffer.capacity()};
  ret.raw_buffer().push_back(buffer.readable());
  return ret;
}
typedef socket_writer_base<fixed_buffer> fixed_socket_writer;
typedef socket_writer_base<flex_buffer> flex_socket_writer;

}  // namespace coring

#endif  // CORING_SOCKET_WRITER_HPP
