///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CORING_ASYNC_DETAIL_LIGHTWEIGHT_MANUAL_RESET_EVENT
#define CORING_ASYNC_DETAIL_LIGHTWEIGHT_MANUAL_RESET_EVENT

#include "config.hpp"

#if CPPCORO_OS_LINUX || (CPPCORO_OS_WINNT >= 0x0602)
#include <atomic>
#include <cstdint>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <linux/futex.h>
#include <cerrno>
#include <climits>
#include <cassert>
#else
#include <mutex>
#include <condition_variable>
#endif

namespace coring {
namespace {
namespace local {
// No futex() function provided by libc.
// Wrap the syscall ourselves here.
int futex(int *UserAddress, int FutexOperation, int Value, const struct timespec *timeout, int *UserAddress2,
          int Value3) {
  return syscall(SYS_futex, UserAddress, FutexOperation, Value, timeout, UserAddress2, Value3);
}
}  // namespace local
}  // namespace

namespace detail {
class lightweight_manual_reset_event {
 public:
  lightweight_manual_reset_event(bool initiallySet = false) : m_value(initiallySet ? 1 : 0) {}

  ~lightweight_manual_reset_event() {}

  void set() noexcept {
    m_value.store(1, std::memory_order_release);

    constexpr int numberOfWaitersToWakeUp = INT_MAX;

    [[maybe_unused]] int numberOfWaitersWokenUp = local::futex(reinterpret_cast<int *>(&m_value), FUTEX_WAKE_PRIVATE,
                                                               numberOfWaitersToWakeUp, nullptr, nullptr, 0);

    // There are no errors expected here unless this class (or the caller)
    // has done something wrong.
    assert(numberOfWaitersWokenUp != -1);
  }

  void reset() noexcept { m_value.store(0, std::memory_order_relaxed); }

  void wait() noexcept {
    // Wait in a loop as futex() can have spurious wake-ups.
    int oldValue = m_value.load(std::memory_order_acquire);
    while (oldValue == 0) {
      int result = local::futex(reinterpret_cast<int *>(&m_value), FUTEX_WAIT_PRIVATE, oldValue, nullptr, nullptr, 0);
      if (result == -1) {
        if (errno == EAGAIN) {
          // The state was changed from zero before we could wait.
          // Must have been changed to 1.
          return;
        }

        // Other errors we'll treat as transient and just read the
        // value and go around the loop again.
      }

      oldValue = m_value.load(std::memory_order_acquire);
    }
  }

 private:
#if CPPCORO_OS_LINUX
  std::atomic<int> m_value;
#elif CPPCORO_OS_WINNT >= 0x0602
  // Windows 8 or newer we can use WaitOnAddress()
  std::atomic<std::uint8_t> m_value;
#elif CPPCORO_OS_WINNT
  // Before Windows 8 we need to use a WIN32 manual reset event.
  coring::detail::win32::handle_t m_eventHandle;
#else
  // For other platforms that don't have a native futex
  // or manual reset event we can just use a std::mutex
  // and std::condition_variable to perform the wait.
  // Not so lightweight, but should be portable to all platforms.
  std::mutex m_mutex;
  std::condition_variable m_cv;
  bool m_isSet;
#endif
};
}  // namespace detail
}  // namespace coring

#endif
