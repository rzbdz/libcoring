
/// This source file is completely moved from
/// https://github.com/CarterLi/liburing4cpp Liscenced with MIT
///
/// author: codeinred
///

#ifndef CORING_UTILS_HPP
#define CORING_UTILS_HPP
#pragma once
#include <unistd.h>
#include <fcntl.h>
#include <array>
#include <string_view>
#include <linux/time_types.h>
#include <system_error>
#include <execinfo.h>
#include <chrono>

#include "../../coring/async/task.hpp"

// these utils won't be exposed to final user
namespace coring::detail {
/** Fill an iovec struct using buf & size */
constexpr inline iovec to_iov(void *buf, size_t size) noexcept { return {buf, size}; }
/** Fill an iovec struct using string view */
constexpr inline iovec to_iov(std::string_view sv) noexcept { return to_iov(const_cast<char *>(sv.data()), sv.size()); }
/** Fill an iovec struct using std::array */
template <size_t N>
constexpr inline iovec to_iov(std::array<char, N> &array) noexcept {
  return to_iov(array.data(), array.size());
}

template <typename Fn>
struct on_scope_exit {
  on_scope_exit(Fn &&fn) : _fn(std::move(fn)) {}
  ~on_scope_exit() { this->_fn(); }

 private:
  Fn _fn;
};

[[nodiscard]] constexpr inline __kernel_timespec dur2ts(std::chrono::nanoseconds dur) noexcept {
  auto secs = std::chrono::duration_cast<std::chrono::seconds>(dur);
  dur -= secs;
  return {secs.count(), dur.count()};
}

/** Convert errno to exception
 * @throw std::runtime_error / std::system_error
 * @return never
 */
[[noreturn]] void panic(std::string_view sv, int err);

struct panic_on_err {
  panic_on_err(std::string_view _command, bool _use_errno) : command(_command), use_errno(_use_errno) {}
  std::string_view command;
  bool use_errno;
};

inline int operator|(int ret, panic_on_err &&poe) {
  if (ret < 0) {
    if (poe.use_errno) {
      panic(poe.command, errno);
    } else {
      if (ret != -ETIME) panic(poe.command, -ret);
    }
  }
  return ret;
}

}  // namespace coring::detail

#endif  // CORING_UTILS_HPP
