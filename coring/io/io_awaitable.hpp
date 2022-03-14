
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
  void resolve(int result) noexcept {
    this->result = result;
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
  detail::io_token *token_ = nullptr;
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

      constexpr bool await_ready() const noexcept { return false; }

      void await_suspend(std::coroutine_handle<> continuation) noexcept {
        token.continuation = continuation;
        io_uring_sqe_set_data(sqe, &token);
      }

      constexpr int await_resume() const noexcept { return token.result; }
    };

    return uio_awaiter(sqe);
  }
  io_cancel_token get_cancel_token() { return {token}; }

 private:
  io_uring_sqe *sqe;
  io_token *token;
};

}  // namespace coring::detail

#endif  // CORING_SQE_AWAITABLE_HPP
