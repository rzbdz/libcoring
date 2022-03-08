///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CORING_ASYNC_IS_AWAITABLE
#define CORING_ASYNC_IS_AWAITABLE

#include <coring/async/detail/get_awaiter.hpp>

#include <type_traits>

namespace coring {
template <typename T, typename = std::void_t<>>
struct is_awaitable : std::false_type {};

template <typename T>
struct is_awaitable<T, std::void_t<decltype(coring::detail::get_awaiter(std::declval<T>()))>> : std::true_type {};

template <typename T>
constexpr bool is_awaitable_v = is_awaitable<T>::value;
}  // namespace coring

#endif
