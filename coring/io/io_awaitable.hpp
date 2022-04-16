
#ifndef CORING_SQE_AWAITABLE_HPP
#define CORING_SQE_AWAITABLE_HPP

#pragma once

#include <liburing.h>
#include <type_traits>
#include <cassert>
#include <functional>
// #include <iostream>
#include <coroutine>
#include "coring/logging/logging.hpp"

namespace coring {
class io_context;
class io_cancel_source;
class io_cancel_token;

namespace detail {
class io_uring_impl;
class io_awaitable_base;
typedef io_awaitable_base io_awaitable;

class io_cancel_control_simple {
 public:
  io_cancel_control_simple() = default;
  explicit io_cancel_control_simple(__u64 io_token) : cancellable_{true}, io_token_{io_token} {}
  [[nodiscard]] bool cancellable() const { return cancellable_; }
  void request_cancel() { is_cancelled_requested_ = true; }
  [[nodiscard]] bool cancel_requested() const { return is_cancelled_requested_; }

  inline io_awaitable do_cancel(io_context &context);

  void set_user_data(__u64 user_data) {
    io_token_ = user_data;
    cancellable_ = true;
  }
  void set_user_data(void *user_data) {
    io_token_ = reinterpret_cast<__u64>(user_data);
    cancellable_ = true;
  }

 private:
  bool cancellable_{false};
  bool is_cancelled_requested_{false};
  __u64 io_token_{0};
};

}  // namespace detail
}  // namespace coring

namespace coring {
class io_cancel_token {
 public:
  typedef detail::io_cancel_control_simple *pointer_t;
  [[nodiscard]] bool cancel_requested() const { return Impl_->cancel_requested(); }
  static io_cancel_token placeholer() { return {nullptr}; }
  operator bool() { return Impl_ != nullptr; }

 private:
  friend class io_cancel_source;
  friend class detail::io_awaitable_base;
  // TODO: friendship with io_uring_impl is an upgly implementation...
  friend class detail::io_uring_impl;
  inline void set_user_data(__u64 user_data) { Impl_->set_user_data(user_data); }
  inline void set_user_data(void *user_data) { Impl_->set_user_data(user_data); }

  io_cancel_token(detail::io_cancel_control_simple *iccs) : Impl_{iccs} {}
  operator void *() { return Impl_; }
  const pointer_t Impl_;
};

class io_cancel_source {
 public:
  io_cancel_source() = default;
  /// Try to cancel, but we don't know if it do the cancel successfully
  /// \param context
  inline void try_cancel(io_context &context);
  /// Cancel a io operation
  /// \param context the io_context that the io operation is depend on
  /// \return If found, the res field of the cqe will contain 0. If not found, res will contain -ENOENT. If found and
  /// attempted cancelled, the res field will contain -EALREADY. In this case, the request may or may not terminate. In
  /// general, requests that are interruptible (like socket IO) will get cancelled, while disk IO requests cannot be
  /// cancelled if already started. Available since 5.5.
  inline detail::io_awaitable cancel_and_wait_for_result(io_context &context);
  auto get_token() { return io_cancel_token{&Impl_}; }

 private:
  detail::io_cancel_control_simple Impl_{0};
};
namespace detail {
// encapsulated to io_uring_context user data as completion token
struct io_token {
  friend struct io_awaitable_base;
  friend struct io_awaitable_flag;
  friend struct io_awaitable_res_flag;

  // result is a int value (same as system call)
  // https://github.com/axboe/liburing/issues/6
  // IO operations are clamped at 2G in Linux
  void resolve(int res, __u32 fl = 0) noexcept {
    this->result = res;
    this->flags = fl;
    // LOG_TRACE("resolve a result, ptr:{}, result: {},flag:{}", continuation.address(), res, fl);
    // std::cout<<"resolve prepare to resume: "<<continuation.address()<<std::endl;
    continuation.resume();
  }

 private:
  std::coroutine_handle<> continuation;
  int result = 0;
  __u32 flags = 0;
};

static_assert(std::is_trivially_destructible_v<io_token>);
}  // namespace detail

namespace detail {

struct io_awaitable_base {
  io_awaitable_base(io_uring_sqe *sqe) noexcept : sqe(sqe), ctoken(io_cancel_token::placeholer()) {
    io_uring_sqe_set_data(sqe, nullptr);
  }
  io_awaitable_base(io_uring_sqe *sqe, io_cancel_token cctoken) noexcept : sqe(sqe), ctoken(cctoken) {
    io_uring_sqe_set_data(sqe, nullptr);
  }
  struct uio_awaiter_base {
    io_token token{};
    io_uring_sqe *sqe;
    io_cancel_token ctoken;
    uio_awaiter_base(io_uring_sqe *sqe, io_cancel_token cctoken) : sqe(sqe), ctoken(cctoken) {}
    constexpr bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> continuation) noexcept {
      token.continuation = continuation;
      if (ctoken) {
        // it's fine if we just pass nullptr in
        // There is a problem here, if the token is dead when we try to co_await this, UB occurs.
        // io_awaitable is complete non-lazy, if user fire it, then at some point he call co_await to get result,
        // what would happend?
        // However, if you don't co_await the awaitable, you don't have a user_data, thus you won't
        // get any result nor cancel it...
        // Conclusion: just fon't fire and forget an io_awaitable...
        ctoken.set_user_data(&token);
      }
      // std::cout << "set coroutine: " << token.continuation.address() << std::endl;
      io_uring_sqe_set_data(sqe, &token);
    }
  };
  auto operator co_await() {
    struct uio_awaiter : uio_awaiter_base {
      uio_awaiter(io_uring_sqe *sqe, io_cancel_token cctoken) : uio_awaiter_base{sqe, cctoken} {}

      constexpr int await_resume() const noexcept { return token.result; }
    };

    return uio_awaiter(sqe, ctoken);
  }

 protected:
  io_uring_sqe *sqe;
  io_cancel_token ctoken;
};

struct io_awaitable_flag : io_awaitable_base {
  io_awaitable_flag(io_uring_sqe *sqe) noexcept : io_awaitable_base{sqe} {}
  io_awaitable_flag(io_uring_sqe *sqe, io_cancel_token cctoken) noexcept : io_awaitable_base{sqe, cctoken} {}

  auto operator co_await() {
    struct uio_awaiter : uio_awaiter_base {
      uio_awaiter(io_uring_sqe *sqe, io_cancel_token cctoken) : uio_awaiter_base{sqe, cctoken} {}

      constexpr int await_resume() const noexcept {
        if (token.result <= 0) {
          return token.result;
        }
        return (int)(token.flags >> IORING_CQE_BUFFER_SHIFT);
      }
    };

    return uio_awaiter(sqe, ctoken);
  }
};

struct io_awaitable_res_flag : io_awaitable_base {
  io_awaitable_res_flag(io_uring_sqe *sqe) noexcept : io_awaitable_base{sqe} {}
  io_awaitable_res_flag(io_uring_sqe *sqe, io_cancel_token cctoken) noexcept : io_awaitable_base{sqe, cctoken} {}

  auto operator co_await() {
    struct uio_awaiter : uio_awaiter_base {
      uio_awaiter(io_uring_sqe *sqe, io_cancel_token cctoken) : uio_awaiter_base{sqe, cctoken} {}

      constexpr std::pair<int, int> await_resume() const noexcept {
        return std::make_pair(token.result, (int)(token.flags >> IORING_CQE_BUFFER_SHIFT));
      }
    };

    return uio_awaiter(sqe, ctoken);
  }
};
}  // namespace detail
}  // namespace coring

#endif  // CORING_SQE_AWAITABLE_HPP
