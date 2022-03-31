
#ifndef CORING_SOCKET_READER_HPP
#define CORING_SOCKET_READER_HPP
#define CORING_EXTRA_THREAD_BUFFER
#include <coroutine>
#include <cstddef>
#include "file_descriptor.hpp"
#include "coring/utils/buffer.hpp"
#include "coring/async/task.hpp"
#include "coring/io/io_context.hpp"
#include "socket.hpp"
namespace coring {  /// This class is a wrapper supporting io_context
/// for char vector based buffer. (just like a decorator)
/// TODO: not a good design for different buffers...
/// But the interfaces is not simple to design.
/// TODO: I am sad that the constructing ways of the socket_reader and socket_writer
/// are widely different.
/// I should make socket_reader support different buffer too,
/// also don't use the awkward function constructor that need
/// to be written manually...
class socket_reader {
 public:
  explicit socket_reader(socket fd, size_t sz = buffer::default_size) : fd_{fd}, upper_layer_{sz} {}
  explicit socket_reader(socket fd, buffer &&container) : fd_{fd}, upper_layer_{std::move(container)} {}

 private:
#ifndef CORING_NO_EXTRA_THREAD_BUFFER
  // TODO: add support for dynamic allocation.
  // TODO: add support for read_fixed,write_fixed
  static thread_local char extra_buffer_[65536];
#endif
 public:
  /// I think it should be same as 'read_all_from_file'
  /// for normal tcp pipe have only 64kb buffer in the kernel
  /// But when it comes to the long-fat one, it won't be the same.
  /// \param
  /// \return how many we read from fd.
  [[nodiscard]] task<int> sync_from_socket();

  task<int> read_some() {
    std::cout << "inside read some" << std::endl;
    co_return co_await sync_from_socket();
  }

  /// read certain bytes. I don't think we need a read_some method
  /// since you can always go and use the raw buffer methods like front()
  /// and readable() to read directly, the use case such as reading directly
  /// to structures are usable.
  /// \param place the destination to place the data
  /// \param nbytes how many you want, it would block the coroutine
  ///        until get the amount data you want, only when an error
  ///        occurs will you get a false return value, which indicates that
  ///        the socket might be closed.
  /// \return
  task<> read_certain(char *place, size_t nbytes) {
    while (upper_layer_.readable() < nbytes) {
      co_await sync_from_socket();
    }
    ::memcpy(place, upper_layer_.front(), nbytes);
    upper_layer_.pop_front(nbytes);
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
      auto read = co_await sync_from_socket();
      if (read <= 0 && errno != EINTR) {
        throw std::runtime_error("socket maybe closed");
      }
      end = upper_layer_.find_eol();
    }
    co_return upper_layer_.pop_string(end - upper_layer_.front() + 1);
  }

  /// If you want to do things directly on the buffer,
  /// just use the raw interfaces.
  /// \return
  task<> read_line(char *place) {
    const char *end = nullptr;
    if (upper_layer_.readable() > 0) {
      end = upper_layer_.find_eol();
    }
    while (end == nullptr) {
      auto read = co_await sync_from_socket();
      if (read <= 0 && errno != EINTR) {
        throw std::runtime_error("socket maybe closed");
      }
      end = upper_layer_.find_eol();
    }
    auto len = end - upper_layer_.front() + 1;
    ::memcpy(place, upper_layer_.front(), len);
    // TODO: I don't know if this is necessary.
    // place[len] = '\0';
    upper_layer_.pop_front(len);
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
      auto read = co_await sync_from_socket();
      if (read <= 0 && errno != EINTR) {
        throw std::runtime_error("socket maybe closed");
      }
      end = upper_layer_.find_crlf();
    }
    co_return upper_layer_.pop_string(end - upper_layer_.front() + 2);
  }

  /// If you want to do things directly on the buffer,
  /// just use the raw interfaces.
  /// \return
  task<> read_crlf_line(char *place) {
    const char *end = nullptr;
    if (upper_layer_.readable() >= 2) {
      end = upper_layer_.find_crlf();
    }
    while (end == nullptr) {
      auto read = co_await sync_from_socket();
      if (read <= 0 && errno != EINTR) {
        throw std::runtime_error("socket maybe closed");
      }
      end = upper_layer_.find_crlf();
    }
    auto len = end - upper_layer_.front() + 2;
    ::memcpy(place, upper_layer_.front(), len);
    // TODO: I don't know if this is necessary.
    // place[len] = '\0';
    upper_layer_.pop_front(len);
  }

  /// If you want to do things directly on the buffer,
  /// just use the raw interfaces.
  /// \return
  task<std::string> read_till_2crlf() {
    const char *end = nullptr;
    if (upper_layer_.readable() >= 4) {
      end = upper_layer_.find_2crlf();
    }
    while (end == nullptr) {
      auto read = co_await sync_from_socket();
      if (read <= 0 && errno != EINTR) {
        throw std::runtime_error("socket maybe closed");
      }
      end = upper_layer_.find_2crlf();
    }
    co_return upper_layer_.pop_string(end - upper_layer_.front() + 4);
  }

  /// If you want to do things directly on the buffer,
  /// just use the raw interfaces.
  /// \return
  task<> read_till_2crlf(char *place) {
    const char *end = nullptr;
    if (upper_layer_.readable() > 0) {
      end = upper_layer_.find_2crlf();
    }
    while (end == nullptr) {
      auto read = co_await sync_from_socket();
      if (read <= 0 && errno != EINTR) {
        throw std::runtime_error("socket maybe closed");
      }
      end = upper_layer_.find_2crlf();
    }
    auto len = end - upper_layer_.front() + 4;
    ::memcpy(place, upper_layer_.front(), len);
    // TODO: I don't know if this is necessary.
    // place[len] = '\0';
    upper_layer_.pop_front(len);
  }
  const buffer &as_buffer() { return upper_layer_; }

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
  buffer upper_layer_;
};
}  // namespace coring

#endif  // CORING_SOCKET_READER_HPP
