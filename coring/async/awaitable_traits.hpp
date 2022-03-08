///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CORING_ASYNC_AWAITABLE_TRAITS
#define CORING_ASYNC_AWAITABLE_TRAITS

#include <coring/async/detail/get_awaiter.hpp>

#include <type_traits>

namespace coring {
template <typename T, typename = void>
struct awaitable_traits {};

template <typename T>
struct awaitable_traits<T, std::void_t<decltype(coring::detail::get_awaiter(std::declval<T>()))>> {
  using awaiter_t = decltype(coring::detail::get_awaiter(std::declval<T>()));

  using await_result_t = decltype(std::declval<awaiter_t>().await_resume());
};
}  // namespace coring

#endif
