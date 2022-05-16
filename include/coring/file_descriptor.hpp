
#ifndef CORING_FILE_DESCRIPTOR_HPP
#define CORING_FILE_DESCRIPTOR_HPP
#include "coring/io_context.hpp"
namespace coring {

class file_descriptor : noncopyable {
 protected:
  int fd_{-1};
  void make_invalid() { fd_ = -1; }

 public:
  file_descriptor(int fd) : fd_(fd) {}
  file_descriptor(file_descriptor &&rhs) : fd_(rhs.fd_) { rhs.fd_ = -1; }
  inline detail::io_awaitable close() {
    auto res = coro::get_io_context_ref().close(fd_);
    make_invalid();
    return res;
  }
  operator int() { return fd_; }
  bool invalid() const { return fd_ < 0; }
  virtual ~file_descriptor() {
    if (!invalid()) {
      // TODO: how do you make sure all async request is done or cancelled?
      // 1. use ASYNC_CANCEL_ANY, 2. we don't care if user always co_awaits for
      // a result, they should catch the error...
      coro::get_io_context_ref().spawn([](int fd) -> async_task<> {
        // we co_await it so that at least we can do logging.
        // and as I say before, we have to support cancel when CANCEL_ANY is available
        // if there are any coroutine couldn't be finished and io_context is going to die...
        co_await coro::get_io_context_ref().close(fd);
      }(fd_));
    }
  }
  /// their is no any useful member functions here,
  /// because I treat this as a trivial wrapper, implementation depends on the fie type
  /// like a regular file and a non-regular file(sockets)...
};

}  // namespace coring
#endif  // CORING_FILE_DESCRIPTOR_HPP
