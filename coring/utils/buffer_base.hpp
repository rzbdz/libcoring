
#ifndef CORING_BUFFER_BASE_HPP
#define CORING_BUFFER_BASE_HPP
#include <cstddef>
#include <cassert>
namespace coring::detail {
/// A buffer base, just some read-only methods
/// that const_buffer and buffer both have.
/// TODO: I don't know what' s a better design
class buffer_base {
 public:
  buffer_base() = default;
  explicit buffer_base(size_t index_read, size_t index_write) : index_read_(index_read), index_write_(index_write) {}
  static constexpr size_t default_size = 128;

 public:
  [[nodiscard]] size_t readable() const { return index_write_ - index_read_; }

  void pop_front(size_t len) {
    assert(len <= readable());
    if (len == readable()) {
      LDR("pop_front %lu, but is larger, clear all", len);
      clear();
    } else {
      index_read_ += len;
      LDR("pop_front %lu, now: con: %lu, pro: %lu", len, index_read_, index_write_);
    }
  }

  void clear() { index_read_ = index_write_ = 0; }

 protected:
  size_t index_read_{0};
  size_t index_write_{0};
};
}  // namespace coring::detail

#endif  // CORING_BUFFER_BASE_HPP
