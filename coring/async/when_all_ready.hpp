///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CORING_ASYNC_WHEN_ALL_READY
#define CORING_ASYNC_WHEN_ALL_READY

#include <coring/async/config.hpp>
#include <coring/async/awaitable_traits.hpp>
#include <coring/async/is_awaitable.hpp>

#include <coring/async/detail/when_all_ready_awaitable.hpp>
#include <coring/async/detail/when_all_task.hpp>
#include <coring/async/detail/unwrap_reference.hpp>

#include <tuple>
#include <utility>
#include <vector>
#include <type_traits>

namespace coring {
template <
    typename... AWAITABLES,
    std::enable_if_t<
        std::conjunction_v<is_awaitable<detail::unwrap_reference_t<std::remove_reference_t<AWAITABLES>>>...>, int> = 0>
[[nodiscard]] CPPCORO_FORCE_INLINE auto when_all_ready(AWAITABLES &&...awaitables) {
  return detail::when_all_ready_awaitable<std::tuple<detail::when_all_task<
      typename awaitable_traits<detail::unwrap_reference_t<std::remove_reference_t<AWAITABLES>>>::await_result_t>...>>(
      std::make_tuple(detail::make_when_all_task(std::forward<AWAITABLES>(awaitables))...));
}

// TODO: Generalise this from vector<AWAITABLE> to arbitrary sequence of awaitable.

template <typename AWAITABLE,
          typename RESULT = typename awaitable_traits<detail::unwrap_reference_t<AWAITABLE>>::await_result_t>
[[nodiscard]] auto when_all_ready(std::vector<AWAITABLE> awaitables) {
  std::vector<detail::when_all_task<RESULT>> tasks;

  tasks.reserve(awaitables.size());

  for (auto &awaitable : awaitables) {
    tasks.emplace_back(detail::make_when_all_task(std::move(awaitable)));
  }

  return detail::when_all_ready_awaitable<std::vector<detail::when_all_task<RESULT>>>(std::move(tasks));
}
}  // namespace coring

#endif
