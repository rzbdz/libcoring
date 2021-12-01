
#ifndef CORING_THREAD_HPP
#define CORING_THREAD_HPP
#include <unistd.h>
#include <syscall.h>
#include <cstdio>

namespace coring::thread {
// internal
extern thread_local int t_cached_tid;
extern thread_local char *t_tid_string_ptr;
extern thread_local int t_tid_string_length;
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
