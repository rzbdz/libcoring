///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CORING_ASYNC_SYNC_WAIT
#define CORING_ASYNC_SYNC_WAIT

#include <coring/async/detail/lightweight_manual_reset_event.hpp>
#include <coring/async/detail/sync_wait_task.hpp>
#include <coring/async/awaitable_traits.hpp>

#include <cstdint>
#include <atomic>

namespace coring {
template <typename AWAITABLE>
auto sync_wait(AWAITABLE &&awaitable) -> typename coring::awaitable_traits<AWAITABLE &&>::await_result_t {
#if CPPCORO_COMPILER_MSVC
  // HACK: Need to explicitly specify template argument to make_sync_wait_task
  // here to work around a bug in MSVC when passing parameters by universal
  // reference to a coroutine which causes the compiler to think it needs to
  // 'move' parameters passed by rvalue reference.
  auto task = detail::make_sync_wait_task<AWAITABLE>(awaitable);
#else
  auto task = detail::make_sync_wait_task(std::forward<AWAITABLE>(awaitable));
#endif
  detail::lightweight_manual_reset_event event;
  task.start(event);
  event.wait();
  return task.result();
}
}  // namespace coring

#endif
