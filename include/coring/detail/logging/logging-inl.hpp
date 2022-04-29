
#ifndef CORING_LOGGING_INL_HPP
#define CORING_LOGGING_INL_HPP
#include "fmt/format.h"
#include "coring/detail/debug.hpp"
#include <functional>

#define CAST_CSTRING
namespace coring::detail {

template <typename T>
struct should_trans : public std::false_type {};

#ifdef CAST_CSTRING
template <>
struct should_trans<const char *> : public std::true_type {};

template <>
struct should_trans<char *> : public std::true_type {};

template <>
struct should_trans<const char *&> : public std::true_type {};

template <>
struct should_trans<char *&> : public std::true_type {};
#endif

template <typename T>
constexpr bool should_trans_v = should_trans<T>::value;

template <typename T>
struct trans_type {
  typedef T use_type;
};

template <>
struct trans_type<char *> {
  typedef std::string use_type;
};

template <>
struct trans_type<const char *> {
  typedef std::string use_type;
};

template <typename T>
auto trans(T v) {
  // make sure pass by value instead of reference
  // it's ok when char* is just a pointer
  using use_type = typename trans_type<T>::use_type;
  static_assert(!std::is_same_v<use_type, T>);
  static_assert(std::is_constructible_v<use_type, T>);
  return use_type(v);
}

template <typename T>
struct should_copy : public std::true_type {};

template <int N>
struct should_copy<const char (&)[N]> : public std::false_type {};

template <typename T>
constexpr bool should_copy_v = should_copy<T>::value;

template <typename... Args>
auto make_args_tuple(Args &&...) requires(sizeof...(Args) == 0) {
  return std::tie();
}

template <typename Arg, typename... Args>
auto make_args_tuple(Arg &&first_arg, Args &&...args) {
  if constexpr (should_copy_v<Arg>) {
    if constexpr (should_trans_v<Arg>) {
      return std::tuple_cat(std::make_tuple(trans(first_arg)), make_args_tuple(args...));
    } else {
      return std::tuple_cat(std::make_tuple(first_arg), make_args_tuple(args...));
    }
  } else {
    return std::tuple_cat(std::forward_as_tuple(first_arg), make_args_tuple(args...));
  }
}

/// Make a tuple since fmt::format_args_store contains both
/// data (type erased) and type info.
/// This approach would only generate tight assembly codes.
/// The data would passed by pointer to the backend.
/// \tparam Args
/// \param fmt use string_view, so compile time syntax
///            test must be done in the very front end
///            using fmt::format_string<Args...> &&fmt, Args &&...args
/// \param args
/// \return
template <typename... Args>
auto make_log_tuple(fmt::string_view fmt, Args &&...args) {
  return std::tuple_cat(std::make_tuple(fmt), make_args_tuple(args...));
}
/// make a task that can be runned in backend
/// the task is for doing fmt formatting.
/// \tparam Args
/// \param fmt use string_view, so compile time syntax
/////          test must be done in the very front end
/////          using fmt::format_string<Args...> &&fmt, Args &&...args
/// \param args
/// \return
template <typename... Args>
auto make_log_task(fmt::string_view fmt, Args &&...args) {
  return [t = make_log_tuple(fmt, args...)](auto &&output_it) -> void {
    std::apply(
        [](auto &&o, fmt::string_view fmt, auto &&...args) { fmt::vformat_to(o, fmt, fmt::make_format_args(args...)); },
        std::tuple_cat(std::tie(output_it), t));
  };
}

}  // namespace coring::detail
#endif  // CORING_LOGGING_INL_HPP