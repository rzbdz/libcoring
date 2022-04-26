
#ifndef CORING_FILE_DESCRIPTOR_HPP
#define CORING_FILE_DESCRIPTOR_HPP
#include "coring/io/io_context.hpp"
namespace coring {

class file_descriptor {
 protected:
  int fd_;

 public:
  file_descriptor(int fd) : fd_(fd) {}
  detail::io_awaitable close() { return coro::get_io_context_ref().close(fd_); }
  operator int() { return fd_; }
  bool invalid() const { return fd_ < 0; }
};

}  // namespace coring
#endif  // CORING_FILE_DESCRIPTOR_HPP
