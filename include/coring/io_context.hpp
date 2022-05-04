
#pragma once
#ifndef CORING_IO_CONTEXT_HPP
#define CORING_IO_CONTEXT_HPP
#include <functional>
#include <thread>
#include <latch>
#include <sys/poll.h>
#include <sys/signalfd.h>

#include <sys/eventfd.h>
#include <deque>

#include "coring/coring_config.hpp"

#include "coring/detail/io/io_uring_context.hpp"
#include "coring/detail/noncopyable.hpp"
#include "coring/detail/debug.hpp"

#include "coring/task.hpp"
#include "coring/async_task.hpp"
#include "coring/when_all.hpp"
#include "coring/async_scope.hpp"
#include "coring/single_consumer_async_auto_reset_event.hpp"

#include "coring/detail/thread.hpp"
#include "coring/execute.hpp"

#include "timer.hpp"
#include "signals.hpp"
#include "sync_wait.hpp"

namespace coring {
class io_context;
struct coro {
  inline static auto get_io_context() {
    auto ptr = reinterpret_cast<coring::io_context *>(io_context_thread_local);
    // NO exception thrown, just make it support nullptr
    return ptr;
  }
  static io_context &get_io_context_ref() {
    auto ptr = reinterpret_cast<coring::io_context *>(io_context_thread_local);
    if (ptr == nullptr) {
      throw std::runtime_error{"no io_context bind"};
    }
    return *ptr;
  }
  inline static thread_local io_context *io_context_thread_local;
  void static provide(io_context *ctx) { io_context_thread_local = ctx; }
  /// Just make sure you are in a coroutine before calling this,
  /// it's not the same spawn as the one in boost::asio.
  inline static void spawn(task<> &&t) {}
};
}  // namespace coring

namespace coring {
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
class io_context : public coring::detail::io_uring_context {
  // use inheritance here because it's too long...
 public:
  typedef io_context *executor_t;

 private:
  typedef coring::task<> my_todo_t;
  static constexpr uint64_t EV_BIG_VALUE = 0x1fffffffffffffff;
  static constexpr uint64_t EV_SMALL_VALUE = 0x1;
  static constexpr uint64_t EV_STOP_MSG = EV_BIG_VALUE;
  static constexpr uint64_t EV_WAKEUP_MSG = EV_SMALL_VALUE;

  void create_eventfd() {
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

  void create_signalfd(signal_set &s) {
    internal_signal_fd_ = ::signalfd(-1, s.get_set(), 0);
    if (internal_signal_fd_ == -1) {
      // TODO: error handling
      // it's another story.
      // either use the exception or simply print some logs and abort.
      // https://isocpp.org/wiki/faq/exceptions#ctors-can-throw
      throw std::runtime_error("o/s fails to allocate more fd");
    }
  }

 public:
  io_context(int entries = 64, uint32_t flags = 0, uint32_t wq_fd = 0)
      : detail::io_uring_context{entries, flags, wq_fd} {
    create_eventfd();
  }

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

  io_context(int entries, io_uring_params p) : detail::io_uring_context{entries, p} { create_eventfd(); }

  io_context(int entries, io_uring_params *p) : detail::io_uring_context{entries, p} { create_eventfd(); }

  void register_signals(signal_set &s, __sighandler_t func = nullptr) {
    create_signalfd(s);
    signal_func_ = func;
  }

 private:
  coring::async_run init_signalfd(__sighandler_t func) {
    struct signalfd_siginfo siginfo {};
    do {
      co_await read(internal_signal_fd_, &siginfo, sizeof(siginfo), 0, 0);
      if (func != nullptr) {
        func(siginfo.ssi_signo);
      }
      if (siginfo.ssi_signo == SIGINT) {
        stopped_ = true;
      }
    } while (!stopped_);
  }
  /// this coroutine should start (and be suspended) at io_context start
  coring::async_run init_eventfd() {
    uint64_t msg;
    while (!stopped_) {
      // Don't use LOG_DEBUG_RAW since it's not thread safe (only use when testing)
      // LOG_DEBUG_RAW("co_await the eventfd!, must be inside of the loop");
      co_await read(internal_event_fd_, &msg, 8, 0, 0);
      // TODO: I don't know if this is a good solution..
      // since strict-ordering memory model are available on current x86 CPUs.
      // But if volatile restrict program to read from memory, it would be costly.
      if (msg >= EV_STOP_MSG) {
        stopped_ = true;
      }
    }
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
  async_run init_timeout_callback() {
    while (!stopped_) {
      while (timer_.has_more_timeouts()) {
        auto tmp = timer_.get_next_expiration();
        co_await timeout(&tmp);
        timer_.handle_events();
      }
      co_await timer_event_;
    }
  }

  void notify(uint64_t msg) {
    if (::write(internal_event_fd_, &msg, sizeof(msg)) == -1) {
      // TODO: do something
      // it's ok since eventfd isn't been read.
      // when it's going to stop, it's fine
      // to stop it synchronously.
      ;
    }
    // TODO: do something (logging)
  }

  void do_todo_list() {
    std::vector<my_todo_t> local_copy{};
    local_copy.swap(todo_list_);
    for (auto &t : local_copy) {
      // user should not pass any long running task in here.
      // that is to say, a task with co_await inside instead of
      // some blocking task...
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
  void do_cancel_list() { throw std::invalid_argument("not available in this channel"); }

 public:
  inline executor_t as_executor() { return this; }

  bool inline on_this_thread() { return reinterpret_cast<decltype(this)>(coro::get_io_context()) == this; }

  /// Immediate issue a one-way-task to run the awaitable.
  /// Make sure you are in the current thread (or, inside of a coroutine running on the io_context).
  /// check io_context::spawn(...).
  template <typename AWAITABLE>
  void execute(AWAITABLE &&awaitable) {
    // now it was call inside of the wait_for_completion,
    // so it would be submitted soon (at next round of run)
    my_scope_.spawn(std::forward<AWAITABLE>(awaitable));
  }

  /// Just don't use this, check io_context::spawn(...).
  void schedule(my_todo_t &&awaitable) { todo_list_.emplace_back(std::move(awaitable)); }

  void schedule(std::vector<my_todo_t> &task_list) {
    if (todo_list_.empty()) {
      std::swap(todo_list_, task_list);
    } else {
      std::move(task_list.begin(), task_list.end(), std::back_inserter(todo_list_));
      task_list.clear();
    }
  }
  /// set timeout
  /// \tparam AWAITABLE
  /// \param awaitable
  template <typename AWAITABLE, typename Duration>
  void run_after(AWAITABLE &&awaitable, Duration &&duration) {}

 public:
  // on the same thread...
  void register_timeout(std::coroutine_handle<> cont, std::chrono::microseconds exp) {
    // TODO: if no this wrapper, the async_run would be exposed to user.
    // Actually this two argument is all POD...no move required.
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
  /// Updates: after some investigation, I think we don't need any multithreading here,
  /// just forbids it. If we need threading, do our own or checkout another branch to try it out.
  ///
  /// \param awaitable a task, must be void return such as task<>
  template <typename AWAITABLE>
  void inline spawn(AWAITABLE &&awaitable) {
    execute(std::forward<AWAITABLE>(awaitable));
  }

  ~io_context() noexcept override {
    // have to free resources
    ::close(internal_event_fd_);
    if (internal_signal_fd_ != -1) {
      ::close(internal_signal_fd_);
    }
    // have to co_await async_scope
    // FIXME: dangerous here.. we lost the scope, memory leaking
    // we can maintain a list of task/coroutine, then destroy then all, but it's not simple to impl.
    // another better solution would be using a timeout for all operation, but it damage the performance for
    // creating lots of linked timeout to the hrtimer in kernel.
    // a better one should be done using the cancellation, we can just keep a list of user_data (maybe using a
    // std::unordered_set) and cancel all of them.
    [a = this]() -> async_run { co_await a->my_scope_.join(); }();
  }

 private:
  /// the best practice might be using a eventfd in io_uring_context
  /// or manage your timing event using a rb-tree timing wheel etc. to
  /// use IORING_OP_TIMEOUT like timerfd in epoll?
  /// more info: @see:https://kernel.dk/io_uring_context-whatsnew.pdf
  void do_run() {
    // bind thread.
    stopped_ = false;
    init_eventfd();
    // do scheduled tasks
    if (internal_signal_fd_ != -1) {
      init_signalfd(signal_func_);
    }
    do_todo_list();
    init_timeout_callback();
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
    coro::provide(this);
    do_run();
    coro::provide(nullptr);
  }

  void run(std::latch &cd) {
    coro::provide(this);
    cd.count_down();
    do_run();
    coro::provide(nullptr);
  }

  void stop() {
    LOG_TRACE("server done!");
    if (reinterpret_cast<coring::io_context *>(coro::get_io_context()) == this) {
      stopped_ = true;
    } else {
      // lazy stop
      // notify this context, then fall into the run()-while loop-break,
      notify(EV_STOP_MSG);
    }
  }

 private:
  // io_uring_context engine inherited from uio
  // a event fd for many uses
  int internal_event_fd_{-1};
  int internal_signal_fd_{-1};
  std::mutex mutex_;
  // no volatile for all changes are made in the same thread
  // not using stop_token for better performance.
  // TODO: should we use a atomic and stop using eventfd msg to demux ?
  bool stopped_{true};
  // for co_spawn
  std::vector<my_todo_t> todo_list_{};
  // TODO: I won't deal with cancellation now...(1)
  // for cancelling
  std::vector<io_cancel_token> to_cancel_list_{};
  coring::async_scope my_scope_{};
  coring::timer timer_{};
  coring::single_consumer_async_auto_reset_event timer_event_;
  __sighandler_t signal_func_{nullptr};
};
inline void co_spawn(task<> &&t) { coro::get_io_context_ref().spawn(std::move(t)); }
}  // namespace coring

#endif  // CORING_IO_CONTEXT_HPP
