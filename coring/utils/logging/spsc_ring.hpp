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
#include <cstdlib>
#include <bits/shared_ptr.h>
#include <cstring>

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
    data_ = ::malloc((sizeof(T)) * (capacity_ + 2 * k_padding));

    static_assert(alignof(spsc_ring<T>) == k_cache_line_size, "not aligned");
    static_assert(sizeof(spsc_ring<T>) >= 3 * k_cache_line_size);
    assert(reinterpret_cast<volatile char *>(&index_read_) - reinterpret_cast<volatile char *>(&index_write_) >=
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
    unsigned long write_index, read_index, free_entries = 0;
    write_index = index_write_;
    while (free_entries == 0) {
      // capture a local view.
      read_index = index_read_;
      free_entries = (mask_ + read_index - write_index);
    }
    assert(free_entries != static_cast<size_t>(0));
    assert((free_entries & (!mask_)) == 0);
    // prepare to swap local view and global
    auto write_index_next = write_index + 1;
    // unsigned wrap around
    new (static_cast<T *>(data_) + (write_index & mask_) + k_padding) T(std::forward<Args>(args)...);
    std::atomic_thread_fence(std::memory_order_release);
    index_write_ = write_index_next;
  }

  template <typename... Args>
  void emplace_back(Args &&...args) {
    emplace(std::forward<Args>(args)...);
  }

  template <typename... Args>
  bool try_emplace(Args &&...args) {
    static_assert(std::is_constructible<T, Args &&...>::value, "T must be constructible with Args&&...");
    // capture a local view.
    unsigned long const write_index = index_write_;
    unsigned long const read_index = index_read_;
    unsigned long free_entries = (mask_ + read_index - write_index);
    if (free_entries == 0) {
      return false;
    }
    // prepare to swap local view and global
    auto write_index_next = write_index + 1;
    // unsigned wrap around
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
    unsigned long read_index = index_read_;
    unsigned long write_index = index_write_;
    unsigned long entries = write_index - read_index;
    if (__glibc_unlikely(entries == 0)) {
      return nullptr;
    }
    return (static_cast<T *>(data_)) + k_padding + (read_index & mask_);
  }

  void pop() noexcept {
    // used in dtor, has to be noexcept (effective cpp)
    static_assert(std::is_nothrow_destructible<T>::value, "T must be nothrow destructible");
    // don't really care atomicity because only one thread would do the increment on r
    // but do care about the destruction of T.
    unsigned long read_index = index_read_;
    unsigned long read_next = read_index + 1;
    (static_cast<T *>(data_)[k_padding + (read_index & mask_)]).~T();
    // make sure nobody can write to it before destruction
    std::atomic_thread_fence(std::memory_order_release);
    index_read_ = read_next;
  }

  size_t size() const noexcept {
    // a not really useful member
    unsigned long read_index = index_read_;
    unsigned long write_index = index_write_;
    // entries
    unsigned long entries = write_index - read_index;
    assert((entries & (~mask_)) == 0);
    return entries;
  }

  bool batch_out(std::vector<T> &v) {
    LOG_DEBUG_RAW("batch out");
    unsigned long read_index = index_read_;
    unsigned long write_index = index_write_;
    // entries
    unsigned long entries = write_index - read_index;
    if (entries == 0) return false;
    v.resize(v.size() + entries);
    auto from = (read_index & mask_);
    // win, simple copying
    if ((write_index & mask_) > (read_index & mask_)) {
      // really fuck me, k_padding!!!!!
      ::memcpy(&v[v.size() - entries], static_cast<T *>(data_) + k_padding + from, entries * sizeof(T));
      LOG_DEBUG_RAW("simple copying, e: %lu, vs: %lu", entries, v.size());
    }  // lost, two phase copying
    else {
      auto part1len = capacity_ - from;
      auto part2len = entries - part1len;
      ::memcpy(&v[v.size() - entries], static_cast<T *>(data_) + k_padding + from, part1len * sizeof(T));
      ::memcpy(&v[v.size() - part2len], static_cast<T *>(data_) + k_padding, part2len * sizeof(T));
      LOG_DEBUG_RAW("e: %lu, v.sz: %lu, part1len: %lu, part2len: %lu", entries, v.size(), part1len, part2len);
    }
    // fuck me forgetting wrap around rules
    auto next_read = read_index + entries;
    std::atomic_thread_fence(std::memory_order_release);
    index_read_ = next_read;
    return true;
  }

  bool empty() const noexcept { return size() == 0; }
  bool full() const noexcept {
    unsigned long const write_index = index_write_;
    unsigned long const read_index = index_read_;
    unsigned long free_entries = (mask_ + read_index - write_index);
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
  // stupid me: miss the volatile
  // https://stackoverflow.com/questions/70195806/why-g-o2-option-make-unsigned-wrap-around-not-working/70196027#70196027
  // the point is that in the dpdk source code there indeed have volatile qualifier
  // I thought the volatile is useless in memory model before (...)
  alignas(k_cache_line_size) volatile size_t index_write_{0};
  // a single cache line in L1
  alignas(k_cache_line_size) volatile size_t index_read_{0};

  // Padding to avoid adjacent allocations to share cache line
  char padding_[k_cache_line_size - sizeof(index_read_)]{};
};

}  // namespace coring

#endif  // CORING_SPSC_RING_HPP
