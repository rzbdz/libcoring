#include "async_buffer.hpp"

thread_local char coring::async_buffer::extra_buffer_[65536];
coring::task<int> coring::async_buffer::read_from_file(int fd) {
  // make sure file is less than no overhead of ioctl. (reduce a syscall)
  char *borrow = extra_buffer_;
  struct iovec vec[2];
  const size_t buf_free = writable();
  vec[0].iov_base = back();
  vec[0].iov_len = buf_free;
  vec[1].iov_base = borrow;
  vec[1].iov_len = sizeof(borrow);
  const int iov_cnt = (buf_free < sizeof(borrow)) ? 2 : 1;
  auto ctx = get_context();
  auto n = co_await ctx->readv(fd, vec, iov_cnt, 0);
  if (n < 0) {
    // throw exception from errno;
  } else if (static_cast<size_t>(n) <= buf_free) {
    push_back(n);
  } else {
    push_back(buf_free);
    push_back(borrow, n - buf_free);
  }
  co_return n;
}
coring::task<int> coring::async_buffer::write_to_file(int fd) {
  auto ctx = get_context();
  auto n = co_await ctx->write(fd, front(), readable(), 0);
  pop_front(n);
  co_return n;
}
coring::task<size_t> coring::async_buffer::write_all_to_file(int fd) {
  size_t n = readable();
  auto ctx = get_context();
  while (n != 0) {
    auto writed = co_await ctx->write(fd, front(), readable(), 0);
    if (writed < 0) {
      throw std::runtime_error("socket closed or sth happened");
    }
    pop_front(writed);
    n -= writed;
  }
  co_return n;
}
