
#ifndef CORING_SQE_AWAITABLE_HPP
#define CORING_SQE_AWAITABLE_HPP

#pragma once

#include <liburing.h>
#include <type_traits>
#include <cassert>
#include <functional>

#include <coroutine>

namespace coring::detail {
// encapsulated to io_uring_context user data as completion token
struct io_token {
  friend struct io_awaitable;

  // result is a int value (same as system call)
  // https://github.com/axboe/liburing/issues/6
  // IO operations are clamped at 2G in Linux
  void resolve(int res) noexcept {
    this->result = res;
    continuation.resume();
  }

 private:
  std::coroutine_handle<> continuation;
  int result = 0;
};

static_assert(std::is_trivially_destructible_v<io_token>);
}  // namespace coring::detail

namespace coring {

struct io_cancel_token {
  typedef detail::io_token *const const_ptr;
  typedef const detail::io_token *const data_t;
  typedef const data_t *const data_ptr_t;
  data_ptr_t token_ptr;
  explicit io_cancel_token(data_ptr_t ptr) : token_ptr{ptr} {}
  bool is_cancellable() const { return *token_ptr != nullptr; }
  [[nodiscard]] void *get_cancel_key() const { return (void *)*token_ptr; }
};

}  // namespace coring

namespace coring::detail {
struct io_awaitable {
  io_awaitable(io_uring_sqe *sqe) noexcept : sqe(sqe) {}

  auto operator co_await() {
    struct uio_awaiter {
      io_token token{};
      io_uring_sqe *sqe;

      uio_awaiter(io_uring_sqe *sqe) : sqe(sqe) {}
      uio_awaiter(io_uring_sqe *sqe, io_token **token_ptr_place) : sqe(sqe) { *token_ptr_place = &token; }

      constexpr bool await_ready() const noexcept { return false; }

      void await_suspend(std::coroutine_handle<> continuation) noexcept {
        token.continuation = continuation;
        io_uring_sqe_set_data(sqe, &token);
      }

      constexpr int await_resume() const noexcept { return token.result; }
    };

    return uio_awaiter(sqe, &token_ptr);
  }
  io_cancel_token get_cancel_token() {
    // TODO: this won't work at all... trying to find a solution
    // a point is when io_awaitable is created, it stat and detached.
    return io_cancel_token{&token_ptr}; }

 private:
  io_uring_sqe *sqe;
  io_token *token_ptr{nullptr};
};

}  // namespace coring::detail

#endif  // CORING_SQE_AWAITABLE_HPP
