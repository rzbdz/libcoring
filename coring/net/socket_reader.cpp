#include "socket_reader.hpp"
#include "config.hpp"
#ifndef CORING_NO_EXTRA_THREAD_BUFFER
thread_local char coring::socket_reader::extra_buffer_[65536];
#endif
void handle_read_error(int ret_code) {
  if (ret_code == 0) {
    throw std::runtime_error("socket may be closed, encounter EOF");
  }
  if (ret_code < 0 && ret_code != -EINTR) {
    throw std::system_error(std::error_code{-ret_code, std::system_category()});
  }
}
coring::task<> coring::socket_reader::read_some() {
#ifndef CORING_NO_EXTRA_THREAD_BUFFER
  auto &ctx = coro::get_io_context_ref();
  // I don't really want to keep this...
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
  if (n <= 0 && n != -EINTR) {
    // TODO: should I throw exception from errno here ?
    // =0 => socket closed (EOF)
    // <0, errno was set, maybe -EINTR
    throw std::runtime_error("socket closed or sth happened trying to read");
  } else if (static_cast<size_t>(n) <= buf_free) {
    upper_layer_.push_back(n);
  } else {
    upper_layer_.push_back(buf_free);
    upper_layer_.push_back(borrow, n - buf_free);
  }
  co_return n;
#else
  // use recv instead of read benefits from internal poll/epoll mechanism (maybe)
  upper_layer_.make_room(CORING_READ_BUFFER_AT_LEAST_WRITABLE);
  int ret = co_await coro::get_io_context_ref().recv(fd_, (void *)upper_layer_.front(), upper_layer_.writable(), 0);
  handle_read_error(ret);
  upper_layer_.push_back(ret);
#endif
}