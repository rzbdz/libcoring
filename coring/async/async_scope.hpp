// Copyright 2017 Lewis Baker
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is furnished
// to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
#ifndef CORING_ASYNC_ASYNC_SCOPE
#define CORING_ASYNC_ASYNC_SCOPE

#include <coring/async/on_scope_exit.hpp>

#include <atomic>
#include <coroutine>
#include <type_traits>
#include <cassert>

namespace coring {
class async_scope {
 public:
  async_scope() noexcept : m_count(1u) {}

  ~async_scope() {
    // scope must be co_awaited before it destructs.
    assert(m_continuation);
  }

  template <typename AWAITABLE>
  void spawn(AWAITABLE &&awaitable) {
    [](async_scope *scope, std::decay_t<AWAITABLE> awaitable) -> oneway_task {
      scope->on_work_started();
      auto decrementOnCompletion = on_scope_exit([scope] { scope->on_work_finished(); });
      co_await std::move(awaitable);
    }(this, std::forward<AWAITABLE>(awaitable));
  }

  // I add this method to make it support `fire and forget but then look back` semantic
  // FYI: https://togithub.com/lewissbaker/cppcoro/issues/145.
  template <typename AWAITABLE, typename Functor>
  void spawn(AWAITABLE &&awaitable, Functor &&f) {
    [](async_scope *scope, std::decay_t<AWAITABLE> awaitable, std::decay_t<Functor> f) -> oneway_task {
      scope->on_work_started();
      auto decrementOnCompletion = on_scope_exit([scope] { scope->on_work_finished(); });
      auto res = co_await std::move(awaitable);
      f(res);
    }(this, std::forward<AWAITABLE>(awaitable), std::forward<Functor>(f));
  }

  [[nodiscard]] auto join() noexcept {
    class awaiter {
      async_scope *m_scope;

     public:
      awaiter(async_scope *scope) noexcept : m_scope(scope) {}

      bool await_ready() noexcept { return m_scope->m_count.load(std::memory_order_acquire) == 0; }

      bool await_suspend(std::coroutine_handle<> continuation) noexcept {
        m_scope->m_continuation = continuation;
        return m_scope->m_count.fetch_sub(1u, std::memory_order_acq_rel) > 1u;
      }

      void await_resume() noexcept {}
    };

    return awaiter{this};
  }

 private:
  void on_work_finished() noexcept {
    if (m_count.fetch_sub(1u, std::memory_order_acq_rel) == 1) {
      m_continuation.resume();
    }
  }

  void on_work_started() noexcept {
    assert(m_count.load(std::memory_order_relaxed) != 0);
    m_count.fetch_add(1, std::memory_order_relaxed);
  }

  struct oneway_task {
    struct promise_type {
      std::suspend_never initial_suspend() { return {}; }
      std::suspend_never final_suspend() noexcept { return {}; }
      void unhandled_exception() { std::terminate(); }
      oneway_task get_return_object() { return {}; }
      void return_void() {}
    };
  };

  std::atomic<size_t> m_count;
  std::coroutine_handle<> m_continuation;
};
}  // namespace coring

#endif
