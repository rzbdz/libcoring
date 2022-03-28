
#ifndef CORING_BUFFERED_HPP
#define CORING_BUFFERED_HPP
#include <cstddef>
#include "coring/utils/file_descriptor.hpp"
#include "async_buffer.hpp"
#include "socket.hpp"
namespace coring::io {
class buffered : public async_buffer {
 public:
  explicit buffered(file_descriptor fd, size_t sz = async_buffer::default_size) : async_buffer{sz}, fd_{fd} {}

 private:
  task<bool> try_read_more() {
    auto read = co_await read_from_file(fd_);
    // I think if we ask for more, which means we are expecting
    // some data, but a closed socket (EOF with ret == 0) is not what we expect,
    // thus it's considered as a error(exception).
    if (read <= 0) {
      co_return false;
    }
    co_return true;
  }
  task<bool> try_write_file() {
    auto write = co_await write_to_file(fd_);
    // I think if we ask for more, which means we are expecting
    // some data, but a closed socket (EOF with ret == 0) is not what we expect,
    // thus it's considered as a error(exception).
    if (write <= 0) {
      co_return false;
    }
    co_return true;
  }

 public:
  task<bool> read_some(char *place, size_t nbytes) {
    while (readable() < nbytes) {
      bool read = co_await try_read_more();
      if (!read) {
        co_return false;
      }
    }
    ::memcpy(place, front(), nbytes);
    pop_front(nbytes);
    co_return true;
  }

  task<std::string> read_line() {
    const char *end = nullptr;
    bool read;
    if (readable() > 0) {
      end = find_eol();
    }
    while (end == nullptr) {
      read = co_await try_read_more();
      if (read == false) {
        throw std::runtime_error("socket maybe closed");
      }
      end = find_eol();
    }
    co_return pop_string(end - front() + 1);
  }

  task<bool> read_line(char *place) {
    const char *end = nullptr;
    bool read;
    if (readable() > 0) {
      end = find_eol();
    }
    while (end == nullptr) {
      read = co_await try_read_more();
      if (read == false) {
        co_return false;
      }
      end = find_eol();
    }
    auto len = end - front() + 1;
    ::memcpy(place, front(), len);
    pop_front(len);
    co_return true;
  }

  task<std::string> read_crlf_line() {
    const char *end = nullptr;
    bool read;
    if (readable() > 0) {
      end = find_crlf();
    }
    while (end == nullptr) {
      read = co_await try_read_more();
      if (read == false) {
        throw std::runtime_error("socket maybe closed");
      }
      end = find_eol();
    }
    co_return pop_string(end - front() + 2);
  }

  task<> read_crlf_line(char *place) {
    const char *end = nullptr;
    bool read;
    if (readable() > 0) {
      end = find_crlf();
    }
    while (end == nullptr) {
      read = co_await try_read_more();
      if (read == false) {
        throw std::runtime_error("socket maybe closed");
      }
      end = find_eol();
    }
    auto len = end - front() + 2;
    ::memcpy(place, front(), len);
    pop_front(len);
  }

  task<> try_flush_file() {
    // TODO: Exception would be thrown inside of the callee,
    // more sophisticated design is need to be considered.
    co_await write_all_to_file(fd_);
  }

  task<> write_some_loosely(const char *place, size_t nbytes) {
    push_back(place, nbytes);
    if (co_await try_write_file() == false) {
      throw std::runtime_error("socket maybe closed");
    }
  }
  task<> write_some_lazily(const char *place, size_t nbytes) {
    push_back(place, nbytes);
    if (readable() >= 64 * 1024) {
      if (co_await try_write_file() == false) {
        throw std::runtime_error("socket maybe closed");
      }
    }
  }
  task<> write_some_strictly(const char *place, size_t nbytes) {
    push_back(place, nbytes);
    co_await try_flush_file();
  }

  task<> write_some(const char *place, size_t nbytes) {
    push_back(place, nbytes);
    co_await try_flush_file();
  }

 private:
  int fd_;
};
}  // namespace coring::io

#endif  // CORING_BUFFERED_HPP
