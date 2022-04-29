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
#ifndef CORING_ASYNC_WHEN_ALL_READY
#define CORING_ASYNC_WHEN_ALL_READY

#include <coring/detail/async/config.hpp>
#include <coring/awaitable_traits.hpp>
#include <coring/is_awaitable.hpp>

#include <coring/detail/async/when_all_ready_awaitable.hpp>
#include <coring/detail/async/when_all_task.hpp>
#include <coring/detail/async/unwrap_reference.hpp>

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
