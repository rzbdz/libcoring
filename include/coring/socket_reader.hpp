
#ifndef CORING_SOCKET_READER_HPP
#define CORING_SOCKET_READER_HPP
#include <coroutine>
#include <cstddef>
#include "coring/coring_config.hpp"
#include "file_descriptor.hpp"
#include "coring/buffer.hpp"
#include "coring/task.hpp"
#include "coring/io_context.hpp"
#include "socket.hpp"
namespace coring {
/// TODO: there are a lots of works to deal with timeout in this class
/// but I may want to refactor it all.
/// This class is a wrapper supporting io_context
/// for char vector based buffer. (just like a decorator)
/// are widely different.
/// I should make socket_reader support different buffer too,
/// also don't use the awkward function constructor that need
/// to be written manually...
template <typename UpperType = buffer>
class socket_reader_base {
 public:
  template <typename... Args>
  explicit socket_reader_base(socket fd, Args &&...args) : fd_{fd}, upper_layer_{std::forward<Args>(args)...} {
    static_assert(std::is_constructible_v<UpperType, Args...>);
  }

 private:
  static void handle_read_error(int ret_code) {
    if (ret_code == 0) {
      throw std::runtime_error("socket may be closed, encounter EOF");
    }
    if (ret_code < 0 && ret_code != -EINTR) {
      throw std::system_error(std::error_code{-ret_code, std::system_category()});
    }
  }

 public:
  /// I think it should be same as 'read_all_from_file'
  /// for normal tcp pipe have only 64kb buffer in the kernel
  /// But when it comes to the long-fat one, it won't be the same.
  /// \param
  /// \return
  [[nodiscard]] task<> read_some() {
    upper_layer_.make_room(READ_BUFFER_AT_LEAST_WRITABLE);
    int ret = co_await fd_.read_some(upper_layer_.back(), upper_layer_.writable());
    // LOG_TRACE("read_some returns: ", ret);
    handle_read_error(ret);
    upper_layer_.has_written(ret);
  }

  template <class Dur>
  [[nodiscard]] task<> read_some(Dur &&dur) {
    upper_layer_.make_room(READ_BUFFER_AT_LEAST_WRITABLE);
    int ret = co_await fd_.read_some(upper_layer_.back(), upper_layer_.writable(), std::forward<Dur>(dur));
    // LOG_TRACE("read_some returns: ", ret);
    handle_read_error(ret);
    upper_layer_.has_written(ret);
  }

  /// read certain bytes. I don't think we need a read_some method
  /// since you can always go and use the raw buffer methods like front()
  /// and readable() to read directly, the use case such as reading directly
  /// to structures are usable.
  /// \param dest the destination to place the data
  /// \param nbytes how many you want, it would block the coroutine
  ///        until get the amount data you want, only when an error
  ///        occurs will you get a false return value, which indicates that
  ///        the socket might be closed.
  /// \return
  task<> read_certain(char *dest, size_t nbytes) {
    while (upper_layer_.readable() < nbytes) {
      co_await read_some();
    }
    ::memcpy(dest, upper_layer_.front(), nbytes);
    upper_layer_.has_read(nbytes);
  }

  task<std::string_view> read_till_certain(size_t nbytes) {
    while (upper_layer_.readable() < nbytes) {
      co_await read_some();
    }
    co_return std::string_view{upper_layer_.front(), nbytes};
  }

  /// I don't know how to design the error handling
  /// when you need the return value used.
  /// I think I 'd better make all methods to
  /// If you want to do things directly on the buffer,
  /// just use the raw interfaces.
  /// hasexcept and then return void.
  /// \return
  task<std::string> read_line() {
    const char *end = nullptr;
    if (upper_layer_.readable() > 0) {
      end = upper_layer_.find_eol();
    }
    while (end == nullptr) {
      co_await read_some();
      end = upper_layer_.find_eol();
    }
    co_return upper_layer_.pop_string(end - upper_layer_.front() + 1);
  }

  /// If you want to do things directly on the buffer,
  /// just use the raw interfaces.
  /// \return
  task<> read_line(char *dest) {
    const char *end = nullptr;
    if (upper_layer_.readable() > 0) {
      end = upper_layer_.find_eol();
    }
    while (end == nullptr) {
      co_await read_some();
      end = upper_layer_.find_eol();
    }
    auto len = end - upper_layer_.front() + 1;
    ::memcpy(dest, upper_layer_.front(), len);
    // TODO: I don't know if this is necessary.
    // place[len] = '\0';
    upper_layer_.has_read(len);
  }

  task<std::string_view> read_till_line(char *dest) {
    const char *end = nullptr;
    if (upper_layer_.readable() > 0) {
      end = upper_layer_.find_eol();
    }
    while (end == nullptr) {
      co_await read_some();
      end = upper_layer_.find_eol();
    }
    auto len = end - upper_layer_.front() + 1;
    co_return std::string_view{upper_layer_.front(), len};
  }

  /// If you want to do things directly on the buffer,
  /// just use the raw interfaces.
  /// \return
  task<std::string> read_crlf_line() {
    const char *end = nullptr;
    if (upper_layer_.readable() >= 2) {
      end = upper_layer_.find_crlf();
    }
    while (end == nullptr) {
      co_await read_some();
      end = upper_layer_.find_crlf();
    }
    co_return upper_layer_.pop_string(end - upper_layer_.front() + 2);
  }

  /// If you want to do things directly on the buffer,
  /// just use the raw interfaces.
  /// \return
  task<> read_crlf_line(char *dest) {
    const char *end = nullptr;
    if (upper_layer_.readable() >= 2) {
      end = upper_layer_.find_crlf();
    }
    while (end == nullptr) {
      co_await read_some();
      end = upper_layer_.find_crlf();
    }
    auto len = end - upper_layer_.front() + 2;
    ::memcpy(dest, upper_layer_.front(), len);
    // TODO: I don't know if this is necessary.
    // place[len] = '\0';
    upper_layer_.has_read(len);
  }

  task<std::string_view> read_till_crlf() {
    const char *end = nullptr;
    if (upper_layer_.readable() >= 2) {
      end = upper_layer_.find_crlf();
    }
    while (end == nullptr) {
      co_await read_some();
      end = upper_layer_.find_crlf();
    }
    auto len = end - upper_layer_.front() + 2;
    co_return std::string_view{upper_layer_.front(), static_cast<std::string_view::size_type>(len)};
  }

  /// If you want to do things directly on the buffer,
  /// just use the raw interfaces.
  /// \return
  task<std::string> read_2crlf_line() {
    const char *end = nullptr;
    if (upper_layer_.readable() >= 4) {
      end = upper_layer_.find_2crlf();
    }
    while (end == nullptr) {
      co_await read_some();
      end = upper_layer_.find_2crlf();
    }
    co_return upper_layer_.pop_string(end - upper_layer_.front() + 4);
  }

  /// If you want to do things directly on the buffer,
  /// just use the raw interfaces.
  /// \return
  task<> read_2crlf_line(char *dest) {
    const char *end = nullptr;
    if (upper_layer_.readable() > 0) {
      end = upper_layer_.find_2crlf();
    }
    while (end == nullptr) {
      co_await read_some();
      end = upper_layer_.find_2crlf();
    }
    auto len = end - upper_layer_.front() + 4;
    ::memcpy(dest, upper_layer_.front(), len);
    // TODO: I don't know if this is necessary.
    // place[len] = '\0';
    upper_layer_.has_read(len);
  }

  task<std::string_view> read_till_2crlf() {
    const char *end = nullptr;
    if (upper_layer_.readable() > 0) {
      end = upper_layer_.find_2crlf();
    }
    while (end == nullptr) {
      co_await read_some();
      end = upper_layer_.find_2crlf();
    }
    auto len = end - upper_layer_.front() + 4;
    co_return std::string_view{upper_layer_.front(), static_cast<std::string_view::size_type>(len)};
  }

  UpperType &as_buffer() { return upper_layer_; }
  const UpperType &as_buffer() const { return upper_layer_; }

 private:
  // TODO: not work yet
  [[maybe_unused]] void register_buffer() {
    // since the buffers should be registered and cancel at a syscall,
    // It's not good to use such a interface.
    // It would be better if we just return a iovec,
    // make user to register all buffers they have.
    // another problem is our buffer would realloc dynamically
    // so there should be a intermedia.
    // Bad story....
    // We'd better write another fixed-size-buffer.
  }

 private:
  socket fd_;
  UpperType upper_layer_;
};

/// wrap a socket with a buffer...
/// I don't know if there are other approach.
/// \return a buffered io, recommended using auto
template <typename... Args>
requires std::is_constructible_v<fixed_buffer, Args...>
inline auto socket_reader(socket so, Args &&...args) {
  return socket_reader_base<fixed_buffer>{so, std::forward<Args>(args)...};
}
/// wrap a socket with a buffer...
/// I don't know if there are other approach.
/// \return a buffered io, recommended using auto
template <typename... Args>
requires std::is_constructible_v<flex_buffer, Args...>
inline auto socket_reader(socket so, Args &&...args) {
  return socket_reader_base<flex_buffer>{so, std::forward<Args>(args)...};
}
typedef socket_reader_base<fixed_buffer> fixed_socket_reader;
typedef socket_reader_base<flex_buffer> flex_socket_reader;
}  // namespace coring

#endif  // CORING_SOCKET_READER_HPP
