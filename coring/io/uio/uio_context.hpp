
#ifndef CORING_UIO_CONTEXT_HPP
#define CORING_UIO_CONTEXT_HPP

#include <liburing/io_uring.h>
#include "coring/async/task.hpp"
#include "uio.hpp"
namespace coring {
class uio_context {
 public:
  /// TODO(pan): of course it need some signaling mechanism
  /// the best practice might be using a eventfd in io_uring
  /// or manage your timing event using a rb-tree timing wheel etc. to
  /// use IORING_OP_TIMEOUT like timerfd in epoll?
  /// more info: @see:https://kernel.dk/io_uring-whatsnew.pdf
  [[noreturn]] void run() {
    while (true) {
      my_io_.wait_for_a_completion_sync();
    }
  }
  /// FIXME: many problems here
  /// TODO(pan): it 's not a good interface
  /// but how to expose the completion queue
  /// to user without data copying ?
  /// \tparam T
  /// \tparam nothrow
  /// \param t
  /// \return
  template <typename T, bool nothrow>
  T run(const coring::task<T> &t) noexcept(nothrow) {
    while (!t.done()) {
      my_io_.wait_for_a_completion_sync();
    }
    return t.get_result();
  }

 private:
  coring::detail::uio my_io_;
};
}  // namespace coring

#endif  // CORING_UIO_CONTEXT_HPP
