#include "thread.hpp"
#include <type_traits>

namespace coring::thread {
thread_local int t_cached_tid = 0;
thread_local char* t_tid_string_ptr;
thread_local int t_tid_string_length = 6;
void cache_tid() {
  if (t_cached_tid == 0) {
    t_cached_tid = static_cast<pid_t>(::syscall(SYS_gettid));
    // it's okay since thread are managed by thread pool
    char *ptr = new char[32];
    t_tid_string_ptr = ptr;
    t_tid_string_length = ::snprintf(ptr, 32, "%5d ", t_cached_tid);
  }
}
int tid() {
  if (__builtin_expect(t_cached_tid == 0, 0)) {
    cache_tid();
  }
  return t_cached_tid;
}
static_assert(std::is_same_v<int, pid_t>, "pid_t should be int");
}  // namespace coring::thread
