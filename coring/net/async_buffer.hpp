
#ifndef CORING_ASYNC_BUFFER_HPP
#define CORING_ASYNC_BUFFER_HPP
#include <coroutine>
#include "coring/utils/buffer.hpp"
#include "coring/async/task.hpp"
#include "coring/io/io_context.hpp"
namespace coring {
/// TODO: make the buffer registered with io_uring
/// But the interfaces is not simple to design.
class async_buffer : public buffer {
 public:
  async_buffer(size_t init_size = buffer::default_size) : buffer(init_size) {}
  decltype(coro::get_io_context()) context_cache_{nullptr};
  auto get_context() -> decltype(coro::get_io_context()) {
    if (context_cache_ == nullptr) {
      context_cache_ = coro::get_io_context();
    }
    return context_cache_;
  }
  // TODO: add support for dynamic allocation.
  // TODO: add support for read_fixed,write_fixed
  static thread_local char extra_buffer_[65536];
  // TODO: not work yet
  [[maybe_unused]] void register_buffer() {
    // since the buffers should be registered and cancel at a syscall,
    // It's not good to use such a interface.
    // It would be better if we just return a iovec,
    // make user to register all buffers they have.
    // another problem is our buffer would realloc dynamically
    // so there should be a intermedia.
    // Bad story....
    // We'd better write another fixed-size-buffer.
  }
  /// I think it should be same as 'read_all_from_file'
  /// for normal tcp pipe have only 64kb buffer in the kernel
  /// But when it comes to the long-fat one, it won't be the same.
  /// \param fd
  /// \return how many we read from fd.
  [[nodiscard]] task<int> read_from_file(int fd) {
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
  /// just one write.
  /// \param fd
  /// \return
  [[nodiscard]] task<int> write_to_file(int fd) {
    auto ctx = get_context();
    auto n = co_await ctx->write(fd, front(), readable(), 0);
    pop_front(n);
    co_return n;
  }
  /// write all to file, we must handle exception
  /// \param fd
  /// \return
  [[nodiscard]] task<int> write_all_to_file(int fd) {
    int n = readable();
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
};
}  // namespace coring
#endif  // CORING_ASYNC_BUFFER_HPP
