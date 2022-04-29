///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CORING_ASYNC_DETAIL_ANY
#define CORING_ASYNC_DETAIL_ANY

namespace coring {
namespace detail {
// Helper type that can be cast-to from any type.
struct any {
  template <typename T>
  any(T &&) noexcept {}
};
}  // namespace detail
}  // namespace coring

#endif
