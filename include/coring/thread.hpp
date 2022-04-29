
#ifndef CORING_THREAD_HPP
#define CORING_THREAD_HPP
#include <unistd.h>
#include <syscall.h>
#include <cstdio>
#include <type_traits>
// For user data (a bad design, will be revised) such as io_context
#define _SET_KEY_DATA_CODE_GEN(NO)                                                                       \
  inline static thread_local void *t_key_data[NO];                                                       \
  inline static void set_key_data(void *data_ptr, int no) { coring::thread::t_key_data[no] = data_ptr; } \
  inline static void *get_key_data(int no) { return coring::thread::t_key_data[no]; }

namespace coring {
struct thread {
  // internal
  inline static thread_local int t_cached_tid = 0;
  inline static thread_local char *t_tid_string_ptr;
  inline static thread_local int t_tid_string_length = 6;

  _SET_KEY_DATA_CODE_GEN(2)

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

}  // namespace coring
#endif  // CORING_THREAD_HPP
