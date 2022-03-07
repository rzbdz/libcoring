#ifndef CORING_BUFFER_HPP
#define CORING_BUFFER_HPP

#include "coring/net/endian.hpp"
#include "coring/utils/debug.hpp"

#include <vector>
#include <string>
#include <cassert>
#include <cstring>
#include <sys/uio.h>
// class test_test_class;
namespace coring {
class buffer {
  static constexpr size_t default_size = 128;

 public:
  explicit buffer(size_t init_size = default_size) : data_(default_size) {}
  // meta
  size_t capacity() const { return data_.capacity(); }
  size_t writable() const { return data_.size() - index_write_; }
  size_t readable() const { return index_write_ - index_read_; }

  void clear() { index_read_ = index_write_ = 0; }
  const char *front() const {
    LOG_DEBUG_RAW("front, return const cannot be modifyied");
    return (data_.data() + index_read_);
  }
  // get int/string from data
  void pop_front(size_t len) {
    assert(len <= readable());
    if (len == readable()) {
      LOG_DEBUG_RAW("pop_front %lu, but is larger, clear all", len);
      clear();
    } else {
      index_read_ += len;
      LOG_DEBUG_RAW("pop_front %lu, now: con: %lu, pro: %lu", len, index_read_, index_write_);
    }
  }

  template <typename IntType>
  IntType pop_int() {
    LOG_DEBUG_RAW("ask pop %lu bytes", sizeof(IntType));
    assert(readable() >= sizeof(IntType));
    IntType ret = 0;
    ::memcpy(&ret, front(), sizeof(IntType));
    pop_front(sizeof(IntType));
    return coring::network_to_host(ret);
  }

  void push_back(size_t len) {
    LOG_DEBUG_RAW("increase length by %lu, ori:", len);
    LOG_B();
    index_write_ += len;
    LOG_DEBUG_RAW("now:");
    LOG_B();
  }
  char *back() {
    LOG_DEBUG_RAW("back, dangerous, %lu", index_write_);
    return (data_.data() + index_write_);
  }

  char *back(size_t want) {
    if (want > writable()) {
      make_room(want);
    }
    return back();
  }

  void push_back(const void *src, size_t len) {
    LOG_DEBUG_RAW("in push_back: ask for %lu, first byte in src %d", len, (int)(*reinterpret_cast<const char *>(src)));
    char *dst = back(len);
    LOG_DEBUG_RAW("back: 0x%lx", (size_t)dst);
    ::memcpy(dst, src, len);
    LOG_DEBUG_RAW("after copy, first byte in dst: %d", (int)(*dst));
    push_back(len);
  }

  // put int/string to data
  template <typename IntType>
  void push_back_int(IntType to_put) {
    LOG_DEBUG_RAW("push_back a int %lu: %lu bytes", (size_t)to_put, sizeof(to_put));
    to_put = coring::host_to_network(to_put);
    LOG_DEBUG_RAW("to put is now : %lu", (size_t)to_put);
    push_back(&to_put, sizeof(IntType));
  }

  std::string pop_string(size_t len) {
    LOG_DEBUG_RAW("pop_front a string: %lu", len);
    if (len > readable()) {
      LOG_DEBUG_RAW("ask for too long!");
      len = readable();
    }
    std::string ret(front(), len);
    pop_front(len);
    return ret;
  }

  void push_back_string(std::string str) {
    size_t len = str.size();
    LOG_DEBUG_RAW("push back a string: %s, %lu", str.data(), len);
    push_back(str.data(), len);
  }

  void make_room(size_t want) {
    LOG_DEBUG_RAW("make room: %lu", want);
    if (index_read_ > 0) {
      size_t now_have = readable();
      ::memmove(data_.data() + index_read_, front(), now_have);
      index_read_ = 0;
      index_write_ = now_have;
      LOG_DEBUG_RAW("make room: move readable %lu", want);
      LOG_B();
    }
    if (writable() < want) {
      data_.resize(index_write_ + want);
    }
    LOG_B();
  }

  // TODO: io_uring !
  // need to be a awaitable
  size_t read_file(int fd) {
    // make sure file is less than no overhead of ioctl. (reduce a syscall)
    char borrow[65536];
    struct iovec vec[2];
    const size_t buf_free = writable();
    vec[0].iov_base = back();
    vec[0].iov_len = buf_free;
    vec[1].iov_base = borrow;
    vec[1].iov_len = sizeof(borrow);
    const int iov_cnt = (buf_free < sizeof(borrow)) ? 2 : 1;
    const ssize_t n = ::readv(fd, vec, iov_cnt);
    if (n < 0) {
      // throw exception from errno;
    } else if (static_cast<size_t>(n) <= buf_free) {
      index_write_ += n;
    } else {
      index_write_ = data_.size();
      push_back(borrow, n - buf_free);
    }
    return n;
  }

  // friend test_test_class;

 private:
  void LOG_B() {
    LOG_INFO("info of buffer: %p, index_con: %lu, index_pro: %lu, sz: %lu, can_r: %lu, can_w: %lu, capa: %lu",
             (void *)(this), index_read_, index_write_, data_.size(), readable(), writable(), capacity());
  }
  std::vector<char> data_;
  size_t index_read_{0};
  size_t index_write_{0};
};
}  // namespace coring
#endif  // CORING_BUFFER_HPP
