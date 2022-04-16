
#ifndef CORING_TIMEOUT_HPP
#define CORING_TIMEOUT_HPP
#include "timeout_awaitable.hpp"
namespace coring {
/// Use literal like 1us, only support us (I think ns is unnecessary)
/// TODO: I think these casting is very roundabout...
/// \tparam Duration std::chrono type.
/// \param expiration I think you can use chrono_literals like 1us.
/// \return Please just co_await it.
template <typename Duration>
detail::timeout_awaitable timeout(Duration expiration, io_context &ctx = coro::get_io_context()) {
  //  LOG_DEBUG_RAW("coawait timeout, duration(us): %ld",
  //                std::chrono::duration_cast<std::chrono::microseconds>(expiration).count());
  return {std::chrono::duration_cast<std::chrono::microseconds>(expiration), ctx};
}

/// TODO: Not support yet (waiting for a usable C++20 timezone support on g++, MSVC won)
/// an alternative:
/// https://stackoverflow.com/questions/52238978/creating-a-stdchronotime-point-from-a-calendar-date-known-at-compile-time
/// \tparam TimePoint
/// \param p
/// \return
template <typename TimePoint>
detail::timeout_awaitable until(TimePoint p) {
  throw std::runtime_error("NOT SUPPORTED YET, reported by coring::until task");
  return {p, coro::get_io_context()};
}
}  // namespace coring
#endif  // CORING_TIMEOUT_HPP
