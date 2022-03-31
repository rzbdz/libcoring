#ifndef CORING_BUFFER_HPP
#define CORING_BUFFER_HPP

#include "coring/net/endian.hpp"
#define LDR(fmt, args...) static_cast<void>(0)
// #define LDR LOG_DEBUG_RAW
#include "coring/utils/debug.hpp"
#include "buffer_base.hpp"
#include <vector>
#include <string>
#include <cassert>
#include <cstring>
#include <sys/uio.h>
#include <iostream>

namespace coring {

class buffer : public detail::buffer_base {
 public:
  // 4 bytes would be a waste...
  constexpr static const char kCRLF[] = "\r\n\r\n";

 public:
  explicit buffer(size_t init_size = default_size) : data_(default_size) { std::cout << "ctor?buffer\n"; }
  explicit buffer(buffer &&rhs) : buffer_base{rhs}, data_{std::move(rhs.data_)} {}

  // meta
  size_t capacity() const { return data_.capacity(); }
  size_t writable() const { return data_.size() - index_write_; }

  const char *front() const {
    LDR("front, return const cannot be modifyied");
    return (data_.data() + index_read_);
  }

  std::string pop_string(size_t len) {
    LDR("pop_front a string: %lu", len);
    if (len > readable()) {
      LDR("ask for too long!");
      len = readable();
    }
    std::string ret(front(), len);
    pop_front(len);
    return ret;
  }

  template <typename IntType>
  IntType pop_int() {
    LDR("ask pop %lu bytes", sizeof(IntType));
    assert(readable() >= sizeof(IntType));
    IntType ret = 0;
    ::memcpy(&ret, front(), sizeof(IntType));
    pop_front(sizeof(IntType));
    return coring::net::network_to_host(ret);
  }

  const char *back() const {
    LDR("back, dangerous, %lu", index_write_);
    return data_.data() + index_write_;
  }
  char *back() { return data_.data() + index_write_; }

  void push_back(size_t len) {
    LDR("increase length by %lu, ori:", len);
    index_write_ += len;
    LDR("now:");
  }

  const char *back(size_t want) {
    if (want > writable()) {
      make_room(want);
    }
    return back();
  }

  void push_back(const void *src, size_t len) {
    LDR("in push_back: ask for %lu, first byte in src %d", len, (int)(*reinterpret_cast<const char *>(src)));
    char *dst = const_cast<char *>(back(len));
    LDR("back: 0x%lx", (size_t)dst);
    ::memcpy(dst, src, len);
    LDR("after copy, first byte in dst: %d", (int)(*dst));
    push_back(len);
  }

  // put int/string to data
  template <typename IntType>
  void push_back_int(IntType to_put) {
    LDR("push_back a int %lu: %lu bytes", (size_t)to_put, sizeof(to_put));
    to_put = coring::net::host_to_network(to_put);
    LDR("to put is now : %lu", (size_t)to_put);
    push_back(&to_put, sizeof(IntType));
  }

  void push_back_string(std::string str) {
    size_t len = str.size();
    LDR("push back a string: %s, %lu", str.data(), len);
    push_back(str.data(), len);
  }
  /// As named
  /// \return the place of first cr (\r)
  const char *find_crlf() const {
    const char *crlf = std::search(front(), back(), kCRLF, kCRLF + 2);
    return crlf == back() ? nullptr : crlf;
  }

  /// as named
  /// \param start
  /// \return the place of first cr (\r)
  const char *find_crlf(const char *start) const {
    const char *crlf = std::search(start, back(), kCRLF, kCRLF + 2);
    return crlf == back() ? nullptr : crlf;
  }

  /// For http header separator (RFC 2616 s4.1)
  /// \return the place of first cr (\r)
  const char *find_2crlf() const {
    const char *crlf = std::search(front(), back(), kCRLF, kCRLF + 4);
    return crlf == back() ? nullptr : crlf;
  }

  /// For http header separator (RFC 2616 s4.1)
  /// \param start
  /// \return the place of first cr (\r)
  const char *find_2crlf(const char *start) const {
    const char *crlf = std::search(start, back(), kCRLF, kCRLF + 4);
    return crlf == back() ? nullptr : crlf;
  }

  /// find a '\n'
  /// \return the place of first lf
  const char *find_eol() const {
    const void *eol = memchr(front(), '\n', readable());
    return static_cast<const char *>(eol);
  }

  /// find a '\n'
  /// \return the place of first lf
  const char *findEOL(const char *start) const {
    const void *eol = memchr(start, '\n', back() - start);
    return static_cast<const char *>(eol);
  }

  void make_room(size_t want) {
    LDR("make room: %lu", want);
    if (index_read_ > 0) {
      size_t now_have = readable();
      ::memmove(data_.data() + index_read_, front(), now_have);
      index_read_ = 0;
      index_write_ = now_have;
      LDR("make room: move readable %lu", want);
    }
    if (writable() < want) {
      data_.resize(index_write_ + want);
    }
  }

 private:
  std::vector<char> data_;
};
}  // namespace coring
#endif  // CORING_BUFFER_HPP
