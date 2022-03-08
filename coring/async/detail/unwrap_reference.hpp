///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CORING_ASYNC_DETAIL_UNWRAP_REFERENCE
#define CORING_ASYNC_DETAIL_UNWRAP_REFERENCE

#include <functional>

namespace coring {
namespace detail {
template <typename T>
struct unwrap_reference {
  using type = T;
};

template <typename T>
struct unwrap_reference<std::reference_wrapper<T>> {
  using type = T;
};

template <typename T>
using unwrap_reference_t = typename unwrap_reference<T>::type;
}  // namespace detail
}  // namespace coring

#endif
