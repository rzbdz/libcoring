
#ifndef CORING_THREAD_HPP
#define CORING_THREAD_HPP
#include <unistd.h>
#include <syscall.h>
#include <cstdio>
// For user data (a bad design, will be revised) such as io_context
#define _SET_KEY_DATA_CODE_GEN(NO)           \
  extern thread_local void *t_key_data[NO];  \
  void set_key_data(void *data_ptr, int no); \
  void *get_key_data(int no);

namespace coring::thread {
// internal
extern thread_local int t_cached_tid;
extern thread_local char *t_tid_string_ptr;
extern thread_local int t_tid_string_length;

_SET_KEY_DATA_CODE_GEN(2)

void cache_tid();

int tid();
inline const char *tid_string() {
  if (__builtin_expect(t_cached_tid == 0, 0)) {
    cache_tid();
  }
  return t_tid_string_ptr;
}
inline int tid_string_length() { return t_tid_string_length; }
inline bool is_main_thread() { return tid() == ::getpid(); }
}  // namespace coring::thread
#endif  // CORING_THREAD_HPP
