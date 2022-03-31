#include "buffered.hpp"
#ifdef CORING_EXTRA_THREAD_BUFFER
thread_local char coring::io::buffered_reader::extra_buffer_[65536];
#endif
coring::task<int> coring::io::buffered_reader::read_from_file() {
  auto &ctx = coro::get_io_context_ref();
#ifdef CORING_EXTRA_THREAD_BUFFER  // I don't really want to keep this...
  // reduce syscalls caused by insufficient buffer writable size.
  char *borrow = extra_buffer_;
  struct iovec vec[2];
  const size_t buf_free = upper_layer_.writable();
  vec[0].iov_base = upper_layer_.back();
  vec[0].iov_len = buf_free;
  vec[1].iov_base = borrow;
  vec[1].iov_len = sizeof(borrow);
  const int iov_cnt = (buf_free < sizeof(borrow)) ? 2 : 1;
  auto n = co_await ctx.readv(fd_, vec, iov_cnt, 0);
  if (n <= 0 && errno != EINTR) {
    // TODO: should I throw exception from errno here ?
    // =0 => socket closed (EOF)
    // <0, errno was set, maybe EINTR
    throw std::runtime_error("socket closed or sth happened trying to read");
  } else if (static_cast<size_t>(n) <= buf_free) {
    upper_layer_.push_back(n);
  } else {
    upper_layer_.push_back(buf_free);
    upper_layer_.push_back(borrow, n - buf_free);
  }
  co_return n;
#else
  co_return co_await ctx.read(fd_, back(), writable(), 0);
#endif
}