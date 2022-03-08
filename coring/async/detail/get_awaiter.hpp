///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CORING_ASYNC_DETAIL_GET_AWAITER
#define CORING_ASYNC_DETAIL_GET_AWAITER

#include <coring/async/detail/is_awaiter.hpp>
#include <coring/async/detail/any.hpp>

namespace coring {
namespace detail {
template <typename T>
auto get_awaiter_impl(T &&value, int) noexcept(noexcept(static_cast<T &&>(value).operator co_await()))
    -> decltype(static_cast<T &&>(value).operator co_await()) {
  return static_cast<T &&>(value).operator co_await();
}

template <typename T>
auto get_awaiter_impl(T &&value, long) noexcept(noexcept(operator co_await(static_cast<T &&>(value))))
    -> decltype(operator co_await(static_cast<T &&>(value))) {
  return operator co_await(static_cast<T &&>(value));
}

template <typename T, std::enable_if_t<coring::detail::is_awaiter<T &&>::value, int> = 0>
T &&get_awaiter_impl(T &&value, coring::detail::any) noexcept {
  return static_cast<T &&>(value);
}

template <typename T>
auto get_awaiter(T &&value) noexcept(noexcept(detail::get_awaiter_impl(static_cast<T &&>(value), 123)))
    -> decltype(detail::get_awaiter_impl(static_cast<T &&>(value), 123)) {
  return detail::get_awaiter_impl(static_cast<T &&>(value), 123);
}
}  // namespace detail
}  // namespace coring

#endif
