
#ifndef CORING_ASYNC_BUFFER_HPP
#define CORING_ASYNC_BUFFER_HPP
#include <coroutine>
#include "coring/utils/buffer.hpp"
#include "coring/async/task.hpp"
#include "coring/io/io_context.hpp"
namespace coring {
/// This class is a wrapper supporting io_context
/// for char vector based buffer. (just like a decorator)
/// TODO: make the buffer registered with io_uring
/// But the interfaces is not simple to design.
class async_buffer : public buffer {
 private:
  decltype(coro::get_io_context()) context_cache_{nullptr};

 public:
  async_buffer(size_t init_size = buffer::default_size) : buffer(init_size) {}

  auto get_context() -> decltype(coro::get_io_context()) {
    if (__glibc_unlikely(context_cache_ == nullptr)) {
      context_cache_ = coro::get_io_context();
    }
    if (context_cache_ == nullptr) {
      // TODO: Question, do we really need a context_cache here ?
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
  [[nodiscard]] task<int> read_from_file(int fd);
  /// just one write.
  /// \param fd
  /// \return
  [[nodiscard]] task<int> write_to_file(int fd);
  /// write all to file, we must handle exception
  /// \param fd
  /// \return
  [[nodiscard]] task<size_t> write_all_to_file(int fd);
};
}  // namespace coring
#endif  // CORING_ASYNC_BUFFER_HPP
