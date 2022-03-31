
#ifndef CORING_BUFFERED_HPP
#define CORING_BUFFERED_HPP
#define CORING_EXTRA_THREAD_BUFFER
#include <coroutine>
#include <cstddef>
#include "coring/utils/file_descriptor.hpp"
#include "coring/utils/buffer.hpp"
#include "coring/async/task.hpp"
#include "coring/io/io_context.hpp"
#include "socket.hpp"
#include "coring/utils/const_buffer.hpp"

namespace coring::io {
template <typename UpperType = buffer>
class buffered_writer {
 public:
  template <typename... Args>
  explicit buffered_writer(file_descriptor fd, Args &&...args)
      : fd_{fd}, upper_layer_{std::forward<Args...>(args...)} {}
  // C++17
  explicit buffered_writer(file_descriptor fd, UpperType &&up) : fd_{fd}, upper_layer_{std::forward<UpperType>(up)} {}

 public:
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
  /// just one write.
  /// \param
  /// \return
  [[nodiscard]] task<int> write_to_file() {
    auto &ctx = coro::get_io_context_ref();
    auto n = co_await ctx.write(fd_, upper_layer_.front(), upper_layer_.readable(), 0);
    if (n <= 0 && errno != EINTR) {
      throw std::runtime_error("socket closed or sth happened trying to write");
    }
    upper_layer_.pop_front(n);
    co_return n;
  }
  /// write all to file, we must handle exception
  /// \param
  /// \return
  [[nodiscard]] task<size_t> write_all_to_file() {
    size_t n = upper_layer_.readable();
    auto &ctx = coro::get_io_context_ref();
    while (n != 0) {
      auto writed = co_await ctx.write(fd_, upper_layer_.front(), upper_layer_.readable(), 0);
      if (writed <= 0 && errno != EINTR) {
        throw std::runtime_error("socket closed or sth happened trying to write");
      }
      upper_layer_.pop_front(writed);
      n -= writed;
    }
    co_return n;
  }

  task<> write_certain_incrementally(const char *place, size_t nbytes) {
    static_assert(std::is_invocable_v);
    upper_layer_.push_back(place, nbytes);
    co_await write_to_file();
  }
  task<> write_certain_loosely(const char *place, size_t nbytes) {
    static_assert(std::is_same_v<UpperType, buffer>);
    upper_layer_.push_back(place, nbytes);
    if (upper_layer_.readable() >= 64 * 1024) {
      co_await write_to_file();
    }
  }

  task<> write_certain_strictly(const char *place, size_t nbytes) {
    upper_layer_.push_back(place, nbytes);
    co_await write_all_to_file();
  }

  task<> write_certain(const char *place, size_t nbytes) {
    upper_layer_.push_back(place, nbytes);
    co_await write_to_file();
  }

 private:
  int fd_;
  UpperType upper_layer_;
};

/// This class is a wrapper supporting io_context
/// for char vector based buffer. (just like a decorator)
/// TODO: not a good design for different buffers...
/// But the interfaces is not simple to design.
class buffered_reader {
 public:
  explicit buffered_reader(file_descriptor fd, size_t sz = buffer::default_size) : fd_{fd}, upper_layer_{sz} {}
  explicit buffered_reader(file_descriptor fd, buffer &&container) : fd_{fd}, upper_layer_{std::move(container)} {}

 private:
#ifdef CORING_EXTRA_THREAD_BUFFER
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
  [[nodiscard]] task<int> read_from_file();
  task<int> read_some() {
    std::cout << "inside read some" << std::endl;
    co_await read_from_file();
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
      co_await read_from_file();
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
      auto read = co_await read_from_file();
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
      auto read = co_await read_from_file();
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
      auto read = co_await read_from_file();
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
      auto read = co_await read_from_file();
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
      auto read = co_await read_from_file();
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
      auto read = co_await read_from_file();
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
  int fd_;
  buffer upper_layer_;
};
typedef buffered_writer<const_buffer> const_writer;

class buffered : public buffered_reader, public buffered_writer<buffer> {
 public:
  explicit buffered(file_descriptor fd, size_t sz = buffer::default_size)
      : buffered_reader{fd, sz}, buffered_writer{fd, sz} {}
  explicit buffered(file_descriptor read_fd, file_descriptor write_fd, size_t sz = buffer::default_size)
      : buffered_reader{read_fd, sz}, buffered_writer{write_fd, sz} {}
  explicit buffered(file_descriptor read_fd, file_descriptor write_fd, size_t rdsz = buffer::default_size,
                    size_t wrsz = buffer::default_size)
      : buffered_reader{read_fd, rdsz}, buffered_writer{write_fd, wrsz} {}
  explicit buffered(file_descriptor read_fd, file_descriptor write_fd, buffer &&rd_buffer, buffer &&wr_buffer)
      : buffered_reader{read_fd, std::move(rd_buffer)}, buffered_writer{write_fd, std::move(wr_buffer)} {}
};
}  // namespace coring::io

#endif  // CORING_BUFFERED_HPP
