/// lock-free single producer single consumer ring buffer (bounded)
///
/// It's simple to implement a lock free SPSC ring buffer for there won't be any contention.
/// But there are some details need to be handled to implement correctly and with high efficiency.
/// This implementation is modeled after Intel DPDK::rte_ring and Linux kernel::kfifo
///
/// design documentation of DPDK::rte_ring:
///   - https://doc.dpdk.org/guides-17.11/prog_guide/ring_lib.html#use-cases
/// related source code :
///   - http://code.dpdk.org/dpdk/v19.11/source/lib/librte_ring/rte_ring.c
///   - https://elixir.bootlin.com/linux/latest/source/include/linux/kfifo.h
/// use alignas to prevent performance strike caused by cache line false sharing
///   -
///   https://www.intel.com/content/www/us/en/develop/documentation/vtune-cookbook/top/tuning-recipes/false-sharing.html
///   - false sharing solution learned from github.com/rigtorp/SPSCQueue and github.com/MengRao/SPSC_Queue

#ifndef CORING_SPSC_RING_HPP
#define CORING_SPSC_RING_HPP

#include <vector>
#include <cassert>
#include <atomic>
#include <bits/shared_ptr.h>

#define RAW_LOG
// #define RAW_NDEBUG

#include "../debug.hpp"
#include "../noncopyable.hpp"
namespace coring {
template <typename T>
class spsc_ring : noncopyable {
 public:
  // capacity must be the power of 2
  explicit spsc_ring(const size_t capacity) noexcept : capacity_(capacity) {
    assert((capacity & 1) != 1);
    // memory layout: [padding] [data] [padding]
    if (capacity_ > SIZE_MAX - 2 * k_padding) {
      capacity_ = SIZE_MAX - 2 * k_padding;
    }
    mask_ = capacity_ - 1;
    // FIXME: here bad_alloc may arise, just let it crash.
    // data_ = new T[capacity_ + 2 * k_padding];
    data_ = ::malloc((sizeof(T)) * capacity_ + 2 * k_padding);

    static_assert(alignof(spsc_ring<T>) == k_cache_line_size, "not aligned");
    static_assert(sizeof(spsc_ring<T>) >= 3 * k_cache_line_size);
    assert(reinterpret_cast<char *>(&index_read_) - reinterpret_cast<char *>(&index_write_) >=
           static_cast<std::ptrdiff_t>(k_cache_line_size));
  }

  ~spsc_ring() {
    while (front()) {
      pop();
    }
    // delete[] data_;
    free(data_);
  }

  template <typename... Args>
  void emplace(Args &&...args) {
    static_assert(std::is_constructible<T, Args &&...>::value, "T must be constructible with Args&&...");
    size_t write_index, read_index, free_entries = 0;
    while (free_entries == 0) {
      // capture a local view.
      write_index = index_write_;
      read_index = index_read_;
      free_entries = (mask_ + read_index - write_index);
    }
    // prepare to swap local view and global
    auto write_index_next = write_index + 1;
    // unsigned wrap around
    new (static_cast<T *>(data_) + (write_index & mask_) + k_padding) T(std::forward<Args>(args)...);
    std::atomic_thread_fence(std::memory_order_release);
    index_write_ = write_index_next;
  }

  template <typename... Args>
  bool try_emplace(Args &&...args) {
    static_assert(std::is_constructible<T, Args &&...>::value, "T must be constructible with Args&&...");
    // capture a local view.
    auto const write_index = index_write_;
    auto const read_index = index_read_;
    auto free_entries = (mask_ + read_index - write_index);
    if (free_entries == 0) {
      return false;
    }
    // prepare to swap local view and global
    auto write_index_next = write_index + 1;
    // unsigned wrap around
    LOG_DEBUG_RAW("writei: %lu, writeinext: %lu", write_index, write_index_next);
    new (static_cast<T *>(data_) + (write_index & mask_) + k_padding) T(std::forward<Args>(args)...);
    // make sure new product in place before being viewed.
    std::atomic_thread_fence(std::memory_order_release);
    index_write_ = write_index_next;
    return true;
  }

  void push(const T &v) {
    static_assert(std::is_copy_constructible<T>::value, "T must be copy constructible");
    emplace(v);
  }

  template <typename P>
  void push(P &&v) {
    emplace(std::forward<P>(v));
  }

  bool try_push(const T &v) {
    static_assert(std::is_copy_constructible<T>::value, "T must be copy constructible");
    return try_emplace(v);
  }

  template <typename P>
  bool try_push(P &&v) {
    return try_emplace(std::forward<P>(v));
  }

  T *front() noexcept {
    // capture local view;
    auto read_index = index_read_;
    auto write_index = index_write_;
    auto entries = write_index - read_index;
    if (__glibc_unlikely(entries == 0)) {
      LOG_DEBUG_RAW("in spsc front is nullptr, w %lu, r %lu", write_index, read_index);
      return nullptr;
    }
    LOG_DEBUG_RAW("in spsc front ready to return:  (read_index & mask_): %lu", (read_index & mask_));
    return &((static_cast<T *>(data_))[k_padding + (read_index & mask_)]);
  }

  void pop() noexcept {
    // used in dtor, has to be noexcept (effective cpp)
    static_assert(std::is_nothrow_destructible<T>::value, "T must be nothrow destructible");
    // don't really care atomicity because only one thread would do the increment on index_read_
    // but do care about the destruction of T.
    auto read_index = index_read_;
    auto read_next = read_index + 1;
    index_read_ = read_next;
    // make sure nobody can read it
    std::atomic_thread_fence(std::memory_order_release);
    (static_cast<T *>(data_)[k_padding + (read_index & mask_)]).~T();
  }

  size_t size() const noexcept {
    // a not really useful member
    auto read_index = index_read_;
    auto write_index = index_write_;
    // entries
    LOG_DEBUG_RAW("spsc size return: %lu, w %lu,r %lu", write_index - read_index, write_index, read_index);
    return write_index - read_index;
  }

  bool empty() const noexcept { return size() == 0; }
  bool full() const noexcept {
    auto const write_index = index_write_;
    auto const read_index = index_read_;
    auto free_entries = (mask_ + read_index - write_index);
    if (free_entries == 0) {
      return false;
    }
    return true;
  }
  size_t capacity() const noexcept {
    // don't forget we have to make one room wasted for the full queue case
    return mask_;
  }
  T *data() { return static_cast<T *>(data_) + k_padding; }

 private:
#ifdef __cpp_lib_hardware_interference_size
  static constexpr size_t k_cache_line_size = std::hardware_destructive_interference_size;
#else
  static constexpr size_t k_cache_line_size = 64;
#endif

  // Padding to avoid false sharing between data_ and adjacent allocations
  static constexpr size_t k_padding = (k_cache_line_size - 1) / sizeof(T) + 1;

 private:
  size_t capacity_;
  size_t mask_;
  void *data_;
  // Align to cache line size in order to avoid false sharing
  // L1 cache: different core has separate cache line, if i_write and i_read
  // in the same line, when producer update read, it would make index_write
  // cannot be independent updated.
  //
  // a single cache line in L1
  alignas(k_cache_line_size) size_t index_write_{0};
  // a single cache line in L1
  alignas(k_cache_line_size) size_t index_read_{0};

  // Padding to avoid adjacent allocations to share cache line
  char padding_[k_cache_line_size - sizeof(index_read_)]{};
};

}  // namespace coring

#endif  // CORING_SPSC_RING_HPP
