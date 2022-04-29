
#ifndef CORING_TIMEOUT_AWAITABLE_HPP
#define CORING_TIMEOUT_AWAITABLE_HPP
#pragma once
#include <coroutine>
#include "io_context.hpp"
namespace coring::detail {
struct timeout_awaitable {
  timeout_awaitable(std::chrono::microseconds t, io_context &c) : timeout{t}, ioc{c} {}
  constexpr bool await_ready() const noexcept { return false; }

  void await_suspend(std::coroutine_handle<> continuation) noexcept { ioc.register_timeout(continuation, timeout); }

  constexpr void await_resume() const noexcept {}

 private:
  std::chrono::microseconds timeout;
  io_context &ioc;
};
}  // namespace coring::detail
#endif  // CORING_TIMEOUT_AWAITABLE_HPP
