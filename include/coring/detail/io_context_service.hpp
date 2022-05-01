// io_context_service.hpp
// Created by PanJunzhong on 2022/4/29.
//

#ifndef CORING_IO_CONTEXT_SERVICE_HPP
#define CORING_IO_CONTEXT_SERVICE_HPP
#include <type_traits>
#include <concepts>
namespace coring {
class io_context;
namespace detail {
template <typename T>
concept IOContextService = requires(T) {
  { T::reference() } -> std::convertible_to<io_context &>;
  { T::pointer() } -> std::convertible_to<io_context *>;
  { T::provide(std::declval<io_context *>()) } -> std::same_as<bool>;
  { T::invalid() } -> std::same_as<void>;
};
}  // namespace detail
}  // namespace coring
#endif  // CORING_IO_CONTEXT_SERVICE_HPP
