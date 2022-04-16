
#pragma once
#ifndef CORING_IO_CONTEXT_HPP
#define CORING_IO_CONTEXT_HPP
#include <functional>
#include <system_error>
#include <chrono>
#include <thread>
#include <latch>
#include <sys/poll.h>
#include <sys/timerfd.h>
#include <sys/eventfd.h>
#include <deque>

#ifndef NDEBUG
#include <execinfo.h>
#endif

#include "coring/coring_config.hpp"

#include "coring/utils/thread.hpp"
#include "coring/utils/time_utils.hpp"
#include "coring/utils/execute.hpp"

#include "coring/async/task.hpp"
#include "coring/async/async_task.hpp"
#include "coring/async/when_all.hpp"
#include "coring/async/async_scope.hpp"
#include "coring/async/single_consumer_async_auto_reset_event.hpp"

#include "io_uring_impl.hpp"
#include "timer.hpp"
#include "coring/async/sync_wait.hpp"

namespace coring {
namespace detail {
constexpr uint64_t EV_BIG_VALUE = 0x1fffffffffffffff;
constexpr uint64_t EV_SMALL_VALUE = 0x1;
}  // namespace detail

///
/// Manual for multi-thread or SQPOLL usage
/// @param entries Maximum sqe can be gotten without submitting
/// @param p a params structure, you can only specify flags which can be used to set various flags and sq_thread_cpu
///  and sq_thread_idle fields
/// < p>p.flags could be one of these (most used):</p>
/// < p>IORING_SETUP_IOPOLL</p>
/// < p>IORING_SETUP_SQPOLL</p>
/// < p>IORING_SETUP_ATTACH_WQ, with this set, the wq_fd should be set to same fd to avoid multiple thread pool</p>
/// < p>other filed of the params are:</p>
/// < p>sq_thread_cpu: the cpuid, you can get one from gcc utility or something, I didn't find detailed document for
///  this field</p>
/// < p>sq_thread_idle: a milliseconds to stop sq polling thread.</p>
///
class io_context : public coring::detail::io_uring_impl {
 public:
  typedef io_context *executor_t;

 private:
  typedef coring::task<> my_task_t;
  static constexpr uint64_t EV_STOP_MSG = detail::EV_BIG_VALUE;
  static constexpr uint64_t EV_WAKEUP_MSG = detail::EV_SMALL_VALUE;

 public:
  explicit io_context(int entries = 64, uint32_t flags = 0, uint32_t wq_fd = 0)
      : detail::io_uring_impl{entries, flags, wq_fd} {
    init();
  }

  io_context(int entries, io_uring_params p) : detail::io_uring_impl{entries, p} { init(); }

  io_context(int entries, io_uring_params *p) : detail::io_uring_impl{entries, p} { init(); }

  /// stupid name, only for dev channel...
  /// \return a io_context isntance.
  static inline io_context dup_from_big_brother(io_context *bro, int entries = 64) {
    auto fl = bro->ring.flags;
    if (fl & IORING_SETUP_SQPOLL) {
      // You can only get real entries from ring->sz or the para when you init one ,we just pass in one...
      // RVO should work here.
      return io_context{entries, fl | IORING_SETUP_ATTACH_WQ, static_cast<uint32_t>(bro->ring_fd())};
    } else {
      return io_context{entries, fl};
    }
  }

 private:
  /// this coroutine should start (and be suspended) at io_context start
  async_task<> init_eventfd() {
    uint64_t msg;
    while (!stopped_) {
      // std::cout << "read eventfd co_await, you see the coroutine address" << std::endl;
      co_await read(internal_event_fd_, &msg, 8, 0, 0);
      // std::cout << "read eventfd wake up" << std::endl;
      // TODO: I don't know if this is a good solution..
      if (msg >= EV_STOP_MSG) stopped_ = true;
    }
    co_return;
  }

  /// TODO: I just find a other approaches to implement user level timer...
  /// In the current version, I just post requests of timeout everytime I wakeup, that is
  /// stupid since there is a api- timeout who rely on a hrtimer in the kernel.
  /// @see https://lwn.net/Articles/800308/
  /// But the order of completions rises by prep_timeout is not guaranteed and thus I think
  /// we do need a user timer...(or just use linked request). Also, when the amount is large,
  /// I don't think the hrtimer(rbtree) would be better (we can do testing though).
  /// The other approach is just use wait_cqe_with_timeout/io_uring_enter,
  /// just like what we did with epoll before.
  /// @see: https://github.com/axboe/liburing/issues/4
  /// \return
  async_task<> init_timeout_callback() {
    while (!stopped_) {
      while (timer_.has_more_timeouts()) {
        auto tmp = timer_.get_next_expiration();
        co_await timeout(&tmp);
        timer_.handle_events();
      }
      co_await timer_event_;
    }
  }

  void notify(uint64_t msg) { ::write(internal_event_fd_, &msg, 8); }

  void do_todo_list() {
    // TODO: do we really need this?
    std::vector<my_task_t> local_copy{};
    {
      std::lock_guard lk(mutex_);
      local_copy.swap(todo_list_);
    }
    for (auto &t : local_copy) {
      // forbidden long-running task
      execute(std::move(t));
    }
  }

  // TODO: I won't deal with cancellation now...(2)
  // Cancellation is complicated, what boost.asio do is
  // just provide interface to cancel all async operations on specific socket (by closing it).
  // Maybe std::stop_source would be enough for most cases.
  // If need to close a socket/file, please make sure you use the IORING_OP_CLOSE with io_uring
  // for revoking all previous posts.
  // @see: https://patchwork.kernel.org/project/linux-fsdevel/patch/20191213183632.19441-9-axboe@kernel.dk/
  void do_cancel_list() {}

  void init() {
    // NONBLOCK for direct write, no influence on io_uring_read
    // internal_event_fd_ = ::eventfd(0, EFD_NONBLOCK);
    internal_event_fd_ = ::eventfd(0, 0);
    if (internal_event_fd_ == -1) {
      // TODO: error handling
      // it's another story.
      // either use the exception or simply print some logs and abort.
      // https://isocpp.org/wiki/faq/exceptions#ctors-can-throw
      throw std::runtime_error("o/s fails to allocate more fd");
    }
  }

 public:
  inline executor_t as_executor() { return this; }

  bool inline on_this_thread() { return reinterpret_cast<decltype(this)>(coring::thread::get_key_data(0)) == this; }

  /// Immediate issue a one-way-task to run the awaitable.
  /// Make sure you are in the current thread (or, inside of a coroutine running on the io_context).
  /// check io_context::spawn(...).
  template <typename AWAITABLE>
  void execute(AWAITABLE &&awaitable) {
    my_scope_.spawn(std::forward<AWAITABLE>(awaitable));
  }

  /// check io_context::spawn(...).
  void schedule(my_task_t &&awaitable) {
    std::lock_guard lk(mutex_);
    todo_list_.emplace_back(std::move(awaitable));
  }

  void schedule(std::vector<my_task_t> &task_list) {
    std::lock_guard lk(mutex_);
    if (todo_list_.empty()) {
      std::swap(todo_list_, task_list);
    } else {
      std::move(task_list.begin(), task_list.end(), std::back_inserter(todo_list_));
      task_list.clear();
    }
  }

  void register_timeout(std::coroutine_handle<> cont, std::chrono::microseconds exp) {
    timer_.add_event(cont, exp);
    timer_event_.set();
  }

  void wakeup() { notify(EV_WAKEUP_MSG); }

  inline void submit() { wakeup(); }

  /// Spawn a task on current event loop
  ///
  /// Since liburing is not thread-safe(especially with io_uring_get_sqe), we
  /// CANNOT spawn anything related to a io_context sqe manipulation outside of
  /// the context thread, usually new task would be spawned when the get_completion
  /// goes to.
  ///
  /// But sometimes user may want to run a coroutine that have nothing to do
  /// with io_uring_context. This should be considered.
  ///
  /// And we can't afford to lock io_uring_get_sqe, that's unacceptable. If you are
  /// going to do that, it means the design of the program is bad.
  ///
  /// TODO: rewrite this for better performance in multi-threaded condition.
  /// Right now this spawn is slow (benched), we should use multiple thread accepting concurrently.
  /// avoiding this slow performance.
  /// I think we should use a lock-free queue for the task queue,
  /// during an interview, the interviewer asked me why I didn't use
  /// lock-free queue for this kind of queues just like what I did in
  /// the async logger....
  /// But the problem I think is due to the 'single' restriction, which
  /// means we need more thread_local memory leakage, so spsc for async
  /// logger should not be used. Actually,
  /// I think we can use mpsc wait-free queue to replace both async_logger
  /// and the todolist queue of io_context. (of course the performance would
  /// be weaker thanks to more CAS operations compared to spsc one,
  /// but it is necessary).
  /// I do have concern on the correctness on lock-free queue, so the queue
  /// in the project is partially modeling the implementation in the intel
  /// DPDK library. What I didn't fork is the multi-producer part of it,
  /// we can take a leap on that.
  /// in case you need it: https://doc.dpdk.org/guides/prog_guide/ring_lib.html
  ///
  /// \tparam thread_check if your task contains a io_uring_context related procedure, and the io_context you
  ///                      use may not running on the current thread
  /// \param awaitable a task, must be void return such as task<>
  template <bool thread_check = false>
  void inline spawn(my_task_t &&awaitable) {
    if constexpr (thread_check) {
      if (on_this_thread()) {
        execute(std::move(awaitable));
      } else {
        schedule(std::move(awaitable));
        // TODO: too many system calls if too many spawn (with thread checking), right now just use spawn<false>.
        wakeup();
      }
    } else {
      execute(std::move(awaitable));
    }
  }

  ~io_context() noexcept override {
    // have to free resources
    ::close(internal_event_fd_);
    // have to co_await async_scope
    sync_wait(my_scope_.join());
  }

 private:
  /// the best practice might be using a eventfd in io_uring_context
  /// or manage your timing event using a rb-tree timing wheel etc. to
  /// use IORING_OP_TIMEOUT like timerfd in epoll?
  /// more info: @see:https://kernel.dk/io_uring_context-whatsnew.pdf
  void do_run() {
    // bind thread.
    stopped_ = false;
    // TODO: just make sure the frame of async_task won't be destroyed
    // we should have more precise management... (maintain a list)
    [[maybe_unused]] auto keep_a_ref = init_eventfd();
    // do scheduled tasks
    do_todo_list();
    // TODO: just make sure the frame of async_task won't be destroyed
    // we should have more precise management... (maintain a list)
    [[maybe_unused]] auto keep_a_ref2 = init_timeout_callback();
    while (!stopped_) {
      // the coroutine would be resumed inside io_token.resolve() method
      // blocking syscall. Call io_uring_submit_and_wait.
      wait_for_completions_then_handle();
      do_todo_list();
    }
    // TODO: handle stop event, deal with async_scope (issue cancellations then call join) exiting
  }

 public:
  void run() {
    coring::thread::set_key_data(this, 0);
    do_run();
    coring::thread::set_key_data(nullptr, 0);
  }

  void run(std::latch &cd) {
    coring::thread::set_key_data(this, 0);
    cd.count_down();
    do_run();
    coring::thread::set_key_data(nullptr, 0);
  }

  void stop() {
    if (reinterpret_cast<coring::io_context *>(coring::thread::get_key_data(0)) == this) {
      stopped_ = true;
    }
    // lazy stop
    // notify this context, then fall into the run()-while loop-break,
    notify(EV_STOP_MSG);
  }

 private:
  // io_uring_context engine inherited from uio
  // a event fd for many uses
  int internal_event_fd_{-1};
  std::mutex mutex_;
  // no volatile for all changes are made in the same thread
  // not using stop_token for better performance.
  // TODO: should we use a atomic and stop using eventfd msg to demux ?
  bool stopped_{true};
  // for co_spawn
  std::vector<my_task_t> todo_list_{};
  // TODO: I won't deal with cancellation now...(1)
  // for cancelling
  std::vector<__u64> to_cancel_list_{};
  coring::async_scope my_scope_{};
  coring::timer timer_{};
  coring::single_consumer_async_auto_reset_event timer_event_;
};
}  // namespace coring

namespace coring {
struct coro {
  static auto get_io_context_ptr() {
    auto ptr = reinterpret_cast<coring::io_context *>(coring::thread::get_key_data(0));
    // NO exception thrown, just make it support nullptr
    return ptr;
  }
  static io_context &get_io_context() {
    auto ptr = reinterpret_cast<coring::io_context *>(coring::thread::get_key_data(0));
    if (ptr == nullptr) {
      throw std::runtime_error{"no io_context bind"};
    }
    return *ptr;
  }
  /// Just make sure you are in a coroutine before calling this,
  /// it's not the same spawn as the one in boost::asio.
  static void spawn(task<> &&t) { get_io_context().spawn(std::move(t)); }
};
}  // namespace coring

namespace coring {
namespace detail {
inline io_awaitable io_cancel_control_simple::do_cancel(io_context &context) {
  if (!cancellable()) {
    // TODO: don't know what to do
    // std::cout << "yield" << std::endl;
    return context.yield();
  }
  if (!cancel_requested()) {
    request_cancel();
  }
  return context.cancel(io_token_);
}
}  // namespace detail
inline void io_cancel_source::try_cancel(io_context &context) { Impl_.do_cancel(context); }
inline detail::io_awaitable io_cancel_source::cancel_and_wait_for_result(io_context &context) {
  return Impl_.do_cancel(context);
}

}  // namespace coring

#endif  // CORING_IO_CONTEXT_HPP
