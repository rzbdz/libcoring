
#ifndef CORING_TIME_UTILS_HPP
#define CORING_TIME_UTILS_HPP

#include <linux/time_types.h>
#include <chrono>
namespace coring {
template <typename Duration>
__kernel_timespec make_timespec(Duration &&duration) {
  __kernel_timespec res;
  // It depends on what unit the tp use.
  auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
  res.tv_sec = duration_ns.count() / 1'000'000'000;
  res.tv_nsec = duration_ns.count() % 1'000'000'000;
  return res;
}
}  // namespace coring
#endif  // CORING_TIME_UTILS_HPP
