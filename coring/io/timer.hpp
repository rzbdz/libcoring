
#ifndef CORING_TIMER_HPP
#define CORING_TIMER_HPP

#include <coroutine>
#include <linux/time_types.h>
#include <chrono>

#include "coring/utils/skiplist_map.hpp"
#include "coring/utils/noncopyable.hpp"
namespace coring {
class timer : noncopyable {
 public:
  // We make a convention that the time_point would be measured in microsecond of epoch.
  typedef long time_point_t;
  static constexpr time_point_t time_point_min_v = std::numeric_limits<time_point_t>::min();
  static constexpr time_point_t time_point_max_v = std::numeric_limits<time_point_t>::max();

 private:
  using nanoseconds = std::chrono::nanoseconds;
  using microseconds = std::chrono::microseconds;
  using system_clock = std::chrono::system_clock;
  struct timer_token {
    std::coroutine_handle<> continuation;
  };
  // TODO: please just use multimap... This would be slow...
  typedef skiplist_map<time_point_t, timer_token, time_point_min_v, time_point_max_v> timer_queue_t;
  timer_queue_t timer_queue_{};

  template <typename Duration>
  __kernel_timespec make_timespec(Duration &&duration) {
    __kernel_timespec res;
    // It depends on what unit the tp use.
    auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
    res.tv_sec = duration_ns.count() / 1'000'000'000;
    res.tv_nsec = duration_ns.count() % 1'000'000'000;
    return res;
  }

 public:
  timer() = default;
  ~timer() = default;
  bool has_more_timeouts() {
    //    LOG_DEBUG_RAW("timer queue sz: %ld", timer_queue_.size());
    return timer_queue_.size() > 0;
  }
  /// call after verified the has_more_timeouts()
  /// \return a timespec(nanoseconds) for io_uring.
  __kernel_timespec get_next_expiration() {
    auto it = timer_queue_.begin();
    // system_clock time_point by default use nanosecond as type
    auto stamp_event = microseconds(it->first);
    // LOG_DEBUG_RAW("stamp event: %ld ms, first: %ld", stamp_event.count(), it->first);
    auto stamp_now = std::chrono::duration_cast<microseconds>(system_clock::now().time_since_epoch());
    auto dur_diff = stamp_event - stamp_now;
    // LOG_DEBUG_RAW("dur diff in get next exp: %ld ms", std::chrono::duration_cast<microseconds>(dur_diff).count());
    // possible overflow
    return make_timespec(max(microseconds::zero(), (dur_diff)));
  }
  int handle_events() {
    if (timer_queue_.size() == 0) {
      return 0;
    }
    auto stamp_now = std::chrono::duration_cast<microseconds>(system_clock::now().time_since_epoch());
    auto to_resume = timer_queue_.pop_less_eq(stamp_now.count());
    //    LOG_DEBUG_RAW("pop less eqed");
    //    timer_queue_.printKey();
    for (auto &t : to_resume) {
      t.continuation.resume();
    }
    auto sz = to_resume.size();
    // BUG: 4 billion timeouts ?
    return static_cast<int>(sz);
  }
  void add_event(std::coroutine_handle<> cont, std::chrono::microseconds timeout) {
    auto stamp_now = std::chrono::duration_cast<microseconds>(system_clock::now().time_since_epoch());
    auto wakeup_point = stamp_now + timeout;
    //    LOG_DEBUG_RAW("timeout %ldus, stamp : %ld wakeuppoint: %ld", timeout.count(), stamp_now.count(),
    //                  wakeup_point.count());
    timer_queue_.insert(wakeup_point.count(), timer_token{cont});
    //    LOG_DEBUG_RAW("add evented");
    //    timer_queue_.printKey();
  }
};
}  // namespace coring

#endif  // CORING_TIMER_HPP
