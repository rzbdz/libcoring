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
#ifndef CORING_ASYNC_WHEN_ALL
#define CORING_ASYNC_WHEN_ALL

#include <coring/async/when_all_ready.hpp>
#include <coring/async/awaitable_traits.hpp>
#include <coring/async/is_awaitable.hpp>
#include <coring/async/fmap.hpp>

#include <coring/async/detail/unwrap_reference.hpp>

#include <tuple>
#include <functional>
#include <utility>
#include <vector>
#include <type_traits>
#include <cassert>

namespace coring {
//////////
// Variadic when_all()

template <
    typename... AWAITABLES,
    std::enable_if_t<
        std::conjunction_v<is_awaitable<detail::unwrap_reference_t<std::remove_reference_t<AWAITABLES>>>...>, int> = 0>
[[nodiscard]] auto when_all(AWAITABLES &&...awaitables) {
  return fmap(
      [](auto &&taskTuple) {
        return std::apply(
            [](auto &&...tasks) { return std::make_tuple(static_cast<decltype(tasks)>(tasks).non_void_result()...); },
            static_cast<decltype(taskTuple)>(taskTuple));
      },
      when_all_ready(std::forward<AWAITABLES>(awaitables)...));
}

//////////
// when_all() with vector of awaitable

template <typename AWAITABLE,
          typename RESULT = typename awaitable_traits<detail::unwrap_reference_t<AWAITABLE>>::await_result_t,
          std::enable_if_t<std::is_void_v<RESULT>, int> = 0>
[[nodiscard]] auto when_all(std::vector<AWAITABLE> awaitables) {
  return fmap(
      [](auto &&taskVector) {
        for (auto &task : taskVector) {
          task.result();
        }
      },
      when_all_ready(std::move(awaitables)));
}

template <typename AWAITABLE,
          typename RESULT = typename awaitable_traits<detail::unwrap_reference_t<AWAITABLE>>::await_result_t,
          std::enable_if_t<!std::is_void_v<RESULT>, int> = 0>
[[nodiscard]] auto when_all(std::vector<AWAITABLE> awaitables) {
  using result_t =
      std::conditional_t<std::is_lvalue_reference_v<RESULT>, std::reference_wrapper<std::remove_reference_t<RESULT>>,
                         std::remove_reference_t<RESULT>>;

  return fmap(
      [](auto &&taskVector) {
        std::vector<result_t> results;
        results.reserve(taskVector.size());
        for (auto &task : taskVector) {
          if constexpr (std::is_rvalue_reference_v<decltype(taskVector)>) {
            results.emplace_back(std::move(task).result());
          } else {
            results.emplace_back(task.result());
          }
        }
        return results;
      },
      when_all_ready(std::move(awaitables)));
}
}  // namespace coring

#endif
