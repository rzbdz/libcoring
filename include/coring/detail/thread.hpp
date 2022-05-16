
#ifndef CORING_THREAD_HPP
#define CORING_THREAD_HPP
#include <unistd.h>
#include <syscall.h>
#include <cstdio>
#include <type_traits>

namespace coring::detail {
struct thread {
  // internal
  inline static thread_local int t_cached_tid = 0;
  inline static thread_local char *t_tid_string_ptr;
  inline static thread_local int t_tid_string_length = 6;

  inline static void cache_tid() {
    if (t_cached_tid == 0) {
      t_cached_tid = static_cast<pid_t>(::syscall(SYS_gettid));
      // it's okay since thread are managed by thread pool
      char *ptr = new char[32];
      t_tid_string_ptr = ptr;
      t_tid_string_length = ::snprintf(ptr, 32, "%5d ", t_cached_tid);
    }
  }

  inline int static tid() {
    if (__builtin_expect(t_cached_tid == 0, 0)) {
      cache_tid();
    }
    return t_cached_tid;
  }

  static inline const char *tid_string() {
    if (__builtin_expect(t_cached_tid == 0, 0)) {
      cache_tid();
    }
    return t_tid_string_ptr;
  }

  inline static int tid_string_length() { return t_tid_string_length; }
  inline static bool is_main_thread() { return tid() == ::getpid(); }
};

static_assert(std::is_same_v<int, pid_t>, "pid_t should be int");

}  // namespace coring::detail
#endif  // CORING_THREAD_HPP
