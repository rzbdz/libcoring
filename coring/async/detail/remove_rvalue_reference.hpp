///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CORING_ASYNC_DETAIL_REMOVE_RVALUE_REFERENCE
#define CORING_ASYNC_DETAIL_REMOVE_RVALUE_REFERENCE

namespace coring {
namespace detail {
template <typename T>
struct remove_rvalue_reference {
  using type = T;
};

template <typename T>
struct remove_rvalue_reference<T &&> {
  using type = T;
};

template <typename T>
using remove_rvalue_reference_t = typename remove_rvalue_reference<T>::type;
}  // namespace detail
}  // namespace coring

#endif
