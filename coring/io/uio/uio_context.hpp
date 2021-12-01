
#ifndef CORING_UIO_CONTEXT_HPP
#define CORING_UIO_CONTEXT_HPP

#include <liburing/io_uring.h>
#include <sys/eventfd.h>
#include <deque>
#include "coring/async/task.hpp"
#include "uio.hpp"
#include "coring/utils/noncopyable.hpp"
namespace coring {
class uio_context : noncopyable {
  using task_handle_t = std::coroutine_handle<>;

 private:
  /// this coroutinue should start (and be suspended) at io_context start
  coring::async_run execute_todo_list() {
    uint64_t msg;
    while (!stopped_) {
      // since handle queue is just a bunch of function pointer in the end, we
      // just use a local copy to avoid high contention.
      std::deque<task_handle_t> local_copy;
      co_await my_io_.read(internal_event_fd_, &msg, 1, 0, 0);
      {
        std::lock_guard lock(mutex_);
        // This exchanges the elements between two deques in constant time.
        // (Four pointers, so it should be quite fast.)
        local_copy.swap(todo_list_);
      }
      for (auto &todo : local_copy) {
        // if there are co_await in the task, it will be suspended (by uio) and return here directly,
        // won't block here.
        todo.resume();
      }
    }
  }

 public:
  uio_context() {
    internal_event_fd_ = ::eventfd(0, 0);
    if (internal_event_fd_ == -1) {
      // TODO: error handling
      // sadly it's another big story to tell.
      // either use the exception or simply print some logs and abort.
      // https://isocpp.org/wiki/faq/exceptions#ctors-can-throw
      throw std::runtime_error("o/s fails to allocate more fd");
    }
    // run the coroutine.
    execute_todo_list();
  }
  ~uio_context() noexcept {
    // have to free resources
    ::close(internal_event_fd_);
  }
  /// TODO: not completed
  /// the best practice might be using a eventfd in io_uring
  /// or manage your timing event using a rb-tree timing wheel etc. to
  /// use IORING_OP_TIMEOUT like timerfd in epoll?
  /// more info: @see:https://kernel.dk/io_uring-whatsnew.pdf
  [[noreturn]] void run() {
    while (!stopped_) {
      my_io_.wait_for_completions();
    }
  }

  void stop() {
    // lazy stop
    stopped_ = true;
  }
  /// FIXME: many problems here
  /// TODO: it 's not a good interface
  /// but how to expose the completion queue
  /// to user without data copying ?
  /// \tparam T
  /// \tparam nothrow
  /// \param t
  /// \return
  template <typename T, bool nothrow>
  T run(const coring::task<T> &t) noexcept(nothrow) {
    while (!t.done()) {
      my_io_.wait_for_completions();
    }
    return t.get_result();
  }

  /// Start a task<T> in the thread that io_context owns.
  /// Similar with the switch_to_new_thread example
  /// in: https://en.cppreference.com/w/cpp/language/coroutines
  /// serve as a scheduler interface (template) for task<T> coroutine to use.
  /// \param todo
  void register_task_handle(task_handle_t todo) {
    std::lock_guard lock(mutex_);
    todo_list_.emplace_back(todo);
  }

 private:
  // io_uring engine!
  coring::detail::uio my_io_;
  // a event fd for many uses
  int internal_event_fd_;
  // it's ok to store handle for it's merely a pointer
  // TODO: locking blocking queue
  // an alternative is to use MPSC lockfree queue(ring buffer).
  // a more simple implementation is to use thread_local SPSC lockfree
  // queue and provide a registering interface exposed to other threads.
  std::deque<task_handle_t> todo_list_;
  std::mutex mutex_;
  bool stopped_{false};
};
}  // namespace coring

#endif  // CORING_UIO_CONTEXT_HPP
