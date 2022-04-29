/// adopted from cppcoro.
/// The original source code (task.hpp) is from cppcoro, Copyright (c) Lewis Baker
/// Modified to be a non-lazy task.
/// Licenced under MIT license.
/// -----------------------------------------------------------------------------
/// Copyright 2017 Lewis Baker (task.hpp)
///
/// Permission is hereby granted, free of charge, to any person obtaining a copy
/// of this software and associated documentation files (the "Software"), to deal
/// in the Software without restriction, including without limitation the rights
/// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
/// copies of the Software, and to permit persons to whom the Software is furnished
/// to do so, subject to the following conditions:
///
/// The above copyright notice and this permission notice shall be included in all
/// copies or substantial portions of the Software.
///
/// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
/// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
/// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
/// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
/// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
/// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
/// SOFTWARE.

#ifndef CORING_CORO_ASYNC_TASK
#define CORING_CORO_ASYNC_TASK

#include <atomic>
#include <exception>
#include <utility>
#include <type_traits>
#include <cstdint>
#include <cassert>
#include <coroutine>
#include "broken_promise.hpp"
#include "coring/detail/async/remove_rvalue_reference.hpp"
#include "coring/awaitable_traits.hpp"

namespace coring {
template <typename T>
class async_task;

namespace detail {
// use base class is to support async_task<> (a.k.a. async_task<void>)
class async_task_promise_base {
  friend struct final_awaitable;

  // for future resumption of suspended routine
  struct final_awaitable {
    bool await_ready() const noexcept { return false; }

    template <typename Promise>
    std::coroutine_handle<> await_suspend(std::coroutine_handle<Promise> async_task_handle) noexcept {
      if (async_task_handle.promise().who_await_me_ && !async_task_handle.promise().who_await_me_.done()) {
        return async_task_handle.promise().who_await_me_;
      } else {
        return std::noop_coroutine();
      }
    }
    void await_resume() noexcept {}
  };

 public:
  async_task_promise_base() noexcept = default;

  auto initial_suspend() noexcept { return std::suspend_never{}; }

  auto final_suspend() noexcept { return final_awaitable{}; }

  void set_continuation(std::coroutine_handle<> h) noexcept { who_await_me_ = h; }

 private:
  std::coroutine_handle<> who_await_me_;
};

template <typename T>
class async_task_promise final : public async_task_promise_base {
 public:
  async_task_promise() noexcept {};

  ~async_task_promise() {
    switch (m_resultType) {
      case result_type::value:
        m_value.~T();
        break;
      case result_type::exception:
        m_exception.~exception_ptr();
        break;
      default:
        break;
    }
  }

  async_task<T> get_return_object() noexcept;

  void unhandled_exception() noexcept {
    ::new (static_cast<void *>(std::addressof(m_exception))) std::exception_ptr(std::current_exception());
    m_resultType = result_type::exception;
  }

  template <typename VALUE, typename = std::enable_if_t<std::is_convertible_v<VALUE &&, T>>>
  void return_value(VALUE &&value) noexcept(std::is_nothrow_constructible_v<T, VALUE &&>) {
    ::new (static_cast<void *>(std::addressof(m_value))) T(std::forward<VALUE>(value));
    m_resultType = result_type::value;
  }

  T &result() & {
    if (m_resultType == result_type::exception) {
      std::rethrow_exception(m_exception);
    }

    assert(m_resultType == result_type::value);

    return m_value;
  }
  // HACK: Need to have co_await of async_task<int> return prvalue rather than
  // rvalue-reference to work around an issue with MSVC where returning
  // rvalue reference of a fundamental type from await_resume() will
  // cause the value to be copied to a temporary. This breaks the
  // sync_wait() implementation.
  // See https://github.com/lewissbaker/cppcoro/issues/40#issuecomment-326864107
  using rvalue_type = std::conditional_t<std::is_arithmetic_v<T> || std::is_pointer_v<T>, T, T &&>;

  rvalue_type result() && {
    if (m_resultType == result_type::exception) {
      std::rethrow_exception(m_exception);
    }

    assert(m_resultType == result_type::value);

    return std::move(m_value);
  }

 private:
  enum class result_type { empty, value, exception };

  result_type m_resultType = result_type::empty;

  union {
    T m_value;
    std::exception_ptr m_exception;
  };
};

template <>
class async_task_promise<void> : public async_task_promise_base {
 public:
  async_task_promise() noexcept = default;

  async_task<void> get_return_object() noexcept;

  void return_void() noexcept {}

  void unhandled_exception() noexcept { m_exception = std::current_exception(); }

  void result() {
    if (m_exception) {
      std::rethrow_exception(m_exception);
    }
  }

 private:
  std::exception_ptr m_exception;
};

template <typename T>
class async_task_promise<T &> : public async_task_promise_base {
 public:
  async_task_promise() noexcept = default;

  async_task<T &> get_return_object() noexcept;

  void unhandled_exception() noexcept { m_exception = std::current_exception(); }

  void return_value(T &value) noexcept { m_value = std::addressof(value); }

  T &result() {
    if (m_exception) {
      std::rethrow_exception(m_exception);
    }

    return *m_value;
  }

 private:
  T *m_value = nullptr;
  std::exception_ptr m_exception;
};
}  // namespace detail

/// \brief
/// A async_task represents an operation that produces a result asynchronously.
/// unlike lazy task, it just run until the first suspended point when it's created.
/// User could get a result later (maybe blocked) by co_await it.
/// A task<> with async_scope would work too, but not as intuitive and have hard time retrieving
/// the results.
/// @see https://github.com/lewissbaker/cppcoro/issues/145
/// @see https://github.com/CarterLi/liburing4cpp/issues/27
///
/// When you call a coroutine that returns a async_task, the coroutine
/// simply captures any passed parameters and execute the task
/// until it reach the first co_await point (that blocks the coroutine), then it
/// returns execution to the caller.
template <typename T = void>
class async_task {
 public:
  using promise_type = detail::async_task_promise<T>;

  using value_type = T;

 private:
  struct awaitable_base {
    std::coroutine_handle<promise_type> task_continuation_;

    explicit awaitable_base(std::coroutine_handle<promise_type> coroutine) noexcept : task_continuation_(coroutine) {}

    [[nodiscard]] bool await_ready() const noexcept {
      // you just need to know task_continuation_ is the coroutine of the async_task...
      // is the async_task is already done, we know, we don't need to save any handle,
      // the control flow now switch to await_resume... that is, return the value to the co_await caller.
      return !task_continuation_ || task_continuation_.done();
    }

    void await_suspend(std::coroutine_handle<> awaitingCoroutine) noexcept {
      // when the async_task is resumed (say by some completion handler), this awaitingCoroutine
      // will be resumed by the final_suspend.

      task_continuation_.promise().set_continuation(awaitingCoroutine);
    }
  };

 public:
  async_task() noexcept : task_continuation_(nullptr) {}

  explicit async_task(std::coroutine_handle<promise_type> coro) : task_continuation_(coro) {}

  async_task(async_task &&t) noexcept : task_continuation_(t.task_continuation_) { t.task_continuation_ = nullptr; }

  /// Disable copy construction/assignment.
  async_task(const async_task &) = delete;
  async_task &operator=(const async_task &) = delete;

  /// Frees resources used by this async_task.
  ~async_task() {
    if (task_continuation_) {
      task_continuation_.destroy();
    }
  }

  async_task &operator=(async_task &&other) noexcept {
    if (std::addressof(other) != this) {
      if (task_continuation_) {
        task_continuation_.destroy();
      }

      task_continuation_ = other.task_continuation_;
      other.task_continuation_ = nullptr;
    }

    return *this;
  }

  /// \brief
  /// Query if the async_task result is complete.
  ///
  /// Awaiting a async_task that is ready is guaranteed not to block/suspend.
  [[nodiscard]] bool is_ready() const noexcept { return !task_continuation_ || task_continuation_.done(); }

  auto operator co_await() const &noexcept {
    struct awaitable : awaitable_base {
      using awaitable_base::awaitable_base;

      decltype(auto) await_resume() {
        if (!this->task_continuation_) {
          throw broken_promise{};
        }
        return this->task_continuation_.promise().result();
      }
    };

    return awaitable{task_continuation_};
  }

  auto operator co_await() const &&noexcept {
    struct awaitable : awaitable_base {
      using awaitable_base::awaitable_base;

      decltype(auto) await_resume() {
        if (!this->task_continuation_) {
          throw broken_promise{};
        }

        return std::move(this->task_continuation_.promise()).result();
      }
    };

    return awaitable{task_continuation_};
  }

  /// \brief
  /// Returns an awaitable that will await completion of the async_task without
  /// attempting to retrieve the result.
  [[nodiscard]] auto when_ready() const noexcept {
    struct awaitable : awaitable_base {
      using awaitable_base::awaitable_base;

      void await_resume() const noexcept {}
    };

    return awaitable{task_continuation_};
  }

 private:
  std::coroutine_handle<promise_type> task_continuation_;
};

namespace detail {
template <typename T>
async_task<T> async_task_promise<T>::get_return_object() noexcept {
  return async_task<T>{std::coroutine_handle<async_task_promise>::from_promise(*this)};
}

inline async_task<void> async_task_promise<void>::get_return_object() noexcept {
  return async_task<void>{std::coroutine_handle<async_task_promise>::from_promise(*this)};
}

template <typename T>
async_task<T &> async_task_promise<T &>::get_return_object() noexcept {
  return async_task<T &>{std::coroutine_handle<async_task_promise>::from_promise(*this)};
}
}  // namespace detail
template <typename Awaitable>
auto make_async_task(Awaitable awaitable) -> async_task<
    coring::detail::remove_rvalue_reference_t<typename coring::awaitable_traits<Awaitable>::await_result_t>> {
  co_return co_await static_cast<Awaitable &&>(awaitable);
}
}  // namespace coring

#endif
