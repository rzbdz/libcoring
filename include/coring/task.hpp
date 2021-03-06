/// adopted from cppcoro.
/// The original source code is from cppcoro, Copyright (c) Lewis Baker
/// Licenced under MIT license.
/// Copyright 2017 Lewis Baker
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
#ifndef CORING_UTILS_CORO_TASK
#define CORING_UTILS_CORO_TASK

#include <atomic>
#include <exception>
#include <utility>
#include <type_traits>
#include <cstdint>
#include <cassert>
#include <coroutine>
#include "coring/logging.hpp"
#include "broken_promise.hpp"
#include "coring/detail/async/remove_rvalue_reference.hpp"
#include "coring/awaitable_traits.hpp"

namespace coring {
template <typename T>
class task;

namespace detail {
// use base class is to support task<> (a.k.a. task<void>)
class task_promise_base {
  friend struct final_awaitable;

  // for future resumption of suspended routine
  struct final_awaitable {
    bool await_ready() const noexcept { return false; }

    template <typename Promise>
    std::coroutine_handle<> await_suspend(std::coroutine_handle<Promise> my_handle) noexcept {
      return my_handle.promise().who_await_me_;
    }
    void await_resume() noexcept {}
  };

 public:
  task_promise_base() noexcept = default;

  auto initial_suspend() noexcept { return std::suspend_always{}; }

  auto final_suspend() noexcept { return final_awaitable{}; }

  void set_continuation(std::coroutine_handle<> h) noexcept { who_await_me_ = h; }

 private:
  std::coroutine_handle<> who_await_me_;
};

template <typename T>
class task_promise final : public task_promise_base {
 public:
  task_promise() noexcept {};

  ~task_promise() {
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

  task<T> get_return_object() noexcept;

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
  // HACK: Need to have co_await of task<int> return prvalue rather than
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
class task_promise<void> : public task_promise_base {
 public:
  task_promise() noexcept = default;

  task<void> get_return_object() noexcept;

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
class task_promise<T &> : public task_promise_base {
 public:
  task_promise() noexcept = default;

  task<T &> get_return_object() noexcept;

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
/// A task represents an operation that produces a result both lazily
/// and asynchronously.
///
/// When you call a coroutine that returns a task, the coroutine
/// simply captures any passed parameters and returns exeuction to the
/// caller. Execution of the coroutine body does not start until the
/// coroutine is first co_await'ed.
template <typename T = void>
class [[nodiscard]] task {
 public:
  using promise_type = detail::task_promise<T>;

  using value_type = T;

 private:
  struct awaitable_base {
    std::coroutine_handle<promise_type> my_continuation_;

    explicit awaitable_base(std::coroutine_handle<promise_type> coroutine) noexcept : my_continuation_(coroutine) {}

    [[nodiscard]] bool await_ready() const noexcept { return !my_continuation_ || my_continuation_.done(); }

    std::coroutine_handle<> await_suspend(std::coroutine_handle<> awaitingCoroutine) noexcept {
      my_continuation_.promise().set_continuation(awaitingCoroutine);
      return my_continuation_;
    }
  };

 public:
  task() noexcept : my_continuation_(nullptr) {}

  explicit task(std::coroutine_handle<promise_type> coro) : my_continuation_(coro) {}

  task(task &&t) noexcept : my_continuation_(t.my_continuation_) { t.my_continuation_ = nullptr; }

  /// Disable copy construction/assignment.
  task(const task &) = delete;
  task &operator=(const task &) = delete;

  /// Frees resources used by this task.
  ~task() {
    if (my_continuation_) {
      // LOG_TRACE("task destruct, address: {}", my_continuation_.address());
      my_continuation_.destroy();
    }
  }

  task &operator=(task &&other) noexcept {
    if (std::addressof(other) != this) {
      if (my_continuation_) {
        my_continuation_.destroy();
      }

      my_continuation_ = other.my_continuation_;
      other.my_continuation_ = nullptr;
    }

    return *this;
  }

  /// \brief
  /// Query if the task result is complete.
  ///
  /// Awaiting a task that is ready is guaranteed not to block/suspend.
  [[nodiscard]] bool is_ready() const noexcept { return !my_continuation_ || my_continuation_.done(); }

  auto operator co_await() const &noexcept {
    struct awaitable : awaitable_base {
      using awaitable_base::awaitable_base;

      decltype(auto) await_resume() {
        if (!this->my_continuation_) {
          throw broken_promise{};
        }

        return this->my_continuation_.promise().result();
      }
    };

    return awaitable{my_continuation_};
  }

  auto operator co_await() const &&noexcept {
    struct awaitable : awaitable_base {
      using awaitable_base::awaitable_base;

      decltype(auto) await_resume() {
        if (!this->my_continuation_) {
          throw broken_promise{};
        }

        return std::move(this->my_continuation_.promise()).result();
      }
    };

    return awaitable{my_continuation_};
  }

  /// \brief
  /// Returns an awaitable that will await completion of the task without
  /// attempting to retrieve the result.
  [[nodiscard]] auto when_ready() const noexcept {
    struct awaitable : awaitable_base {
      using awaitable_base::awaitable_base;

      void await_resume() const noexcept {}
    };

    return awaitable{my_continuation_};
  }

 private:
  std::coroutine_handle<promise_type> my_continuation_;
};

struct async_run {
  struct promise_type {
    async_run get_return_object() { return {}; }
    std::suspend_never initial_suspend() { return {}; }
    std::suspend_never final_suspend() noexcept { return {}; }
    void unhandled_exception() {}
    void return_void() {}
  };
};

namespace detail {
template <typename T>
task<T> task_promise<T>::get_return_object() noexcept {
  return task<T>{std::coroutine_handle<task_promise>::from_promise(*this)};
}

inline task<void> task_promise<void>::get_return_object() noexcept {
  return task<void>{std::coroutine_handle<task_promise>::from_promise(*this)};
}

template <typename T>
task<T &> task_promise<T &>::get_return_object() noexcept {
  return task<T &>{std::coroutine_handle<task_promise>::from_promise(*this)};
}
}  // namespace detail
template <typename Awaitable>
auto make_task(Awaitable awaitable)
    -> task<coring::detail::remove_rvalue_reference_t<typename coring::awaitable_traits<Awaitable>::await_result_t>> {
  co_return co_await static_cast<Awaitable &&>(awaitable);
}
}  // namespace coring

#endif
