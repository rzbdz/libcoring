#ifndef CORING_BUFFER_HPP
#define CORING_BUFFER_HPP

#include "coring/net/endian.hpp"
#define LDR(fmt, args...) static_cast<void>(0)
// #define LDR LOG_DEBUG_RAW
#include "coring/utils/debug.hpp"
#include <vector>
#include <string>
#include <cassert>
#include <cstring>
#include <sys/uio.h>
#include <iostream>

namespace coring {

// 4 bytes would be a waste...
constexpr static const char kCRLF[] = "\r\n\r\n";
/// for const char *, char array, just the data to send
class const_buffer {
 public:
  const_buffer(const char *s, size_t len) : data_{s}, end_{len} {}
  template <size_t N>
  const_buffer(const char (&s)[N]) : data_{s}, end_{N - 1} {
    // truncate the '\0' end flag.
  }
  const_buffer(const std::vector<char> &s) : data_{s.data()}, end_{s.size()} {}

 public:
  const_buffer &operator+=(size_t rhs) {
    rhs = rhs > end_ ? end_ : rhs;
    data_ += rhs;
    end_ -= rhs;
    return *this;
  }

  friend const_buffer operator+(const_buffer &buf, size_t rhs) {
    const_buffer tmp{buf.data_, buf.end_};
    tmp += rhs;
    return tmp;
  }

 public:
  [[nodiscard]] size_t readable() const { return end_ - index_read_; }

  const char *front() const { return data_ + index_read_; }

  const char *back() const { return data_ + end_; }

  void pop_front(size_t len) {
    assert(len <= readable());
    if (len >= readable()) {
      LDR("pop_front %lu, but is larger, clear all", len);
      index_read_ = end_;
    } else {
      index_read_ += len;
      LDR("pop_front %lu, now: con: %lu, pro: %lu", len, index_read_, index_write_);
    }
  }

  void rollback(size_t len) {
    if (len >= index_read_) {
      index_read_ = 0;
    } else {
      index_read_ -= len;
    }
  }

  /// a cycle
  void rollback() { index_read_ = 0; }

  /// If user need to reassemble the data,
  /// say add some separator or do some escaping...
  /// this will be useful.
  std::string pop_string(size_t len) {
    if (len > readable()) {
      len = readable();
    }
    std::string ret(front(), len);
    pop_front(len);
    return ret;
  }

  /// If user need to reassemble the data,
  /// say add some separator or do some escaping...
  /// this will be useful.
  template <typename IntType>
  IntType pop_int() {
    assert(readable() >= sizeof(IntType));
    IntType ret = 0;
    ::memcpy(&ret, front(), sizeof(IntType));
    pop_front(sizeof(IntType));
    return coring::net::network_to_host(ret);
  }
  /// As named
  /// If user need to reassemble the data,
  /// say add some separator or do some escaping...
  /// this will be useful.
  /// \return the place of first cr (\r)
  const char *find_crlf() const {
    const char *crlf = std::search(front(), back(), kCRLF, kCRLF + 2);
    return crlf == back() ? nullptr : crlf;
  }

  /// as named
  /// If user need to reassemble the data,
  /// say add some separator or do some escaping...
  /// this will be useful.
  /// \param start
  /// \return the place of first cr (\r)
  const char *find_crlf(const char *start) const {
    const char *crlf = std::search(start, back(), kCRLF, kCRLF + 2);
    return crlf == back() ? nullptr : crlf;
  }

  /// For http header separator (RFC 2616 s4.1)
  /// If user need to reassemble the data,
  /// say add some separator or do some escaping...
  /// this will be useful.
  /// \return the place of first cr (\r)
  const char *find_2crlf() const {
    const char *crlf = std::search(front(), back(), kCRLF, kCRLF + 4);
    return crlf == back() ? nullptr : crlf;
  }

  /// For http header separator (RFC 2616 s4.1)
  /// If user need to reassemble the data,
  /// say add some separator or do some escaping...
  /// this will be useful.
  /// \param start
  /// \return the place of first cr (\r)
  const char *find_2crlf(const char *start) const {
    const char *crlf = std::search(start, back(), kCRLF, kCRLF + 4);
    return crlf == back() ? nullptr : crlf;
  }

  /// find a '\n'
  /// If user need to reassemble the data,
  /// say add some separator or do some escaping...
  /// this will be useful.
  /// \return the place of first lf
  const char *find_eol() const {
    const void *eol = memchr(front(), '\n', readable());
    return static_cast<const char *>(eol);
  }

  /// find a '\n'
  /// If user need to reassemble the data,
  /// say add some separator or do some escaping...
  /// this will be useful.
  /// \return the place of first lf
  const char *findEOL(const char *start) const {
    const void *eol = memchr(start, '\n', back() - start);
    return static_cast<const char *>(eol);
  }

 protected:
  const char *data_;
  size_t end_;
  size_t index_read_{0};
};

class flex_buffer {
 public:
  static constexpr size_t default_size = 128;

 public:
  explicit flex_buffer(size_t init_size = default_size) : data_(default_size) {}
  explicit flex_buffer(flex_buffer &&rhs)
      : data_{std::move(rhs.data_)}, index_read_{rhs.index_read_}, index_write_{rhs.index_write_} {}

 public:
  [[nodiscard]] size_t readable() const { return index_write_ - index_read_; }
  size_t capacity() const { return data_.capacity(); }
  size_t writable() const { return data_.size() - index_write_; }

 public:
  void clear() { index_read_ = index_write_ = 0; }

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

  [[nodiscard]] const char *back() const {
    LDR("back, dangerous, %lu", index_write_);
    return data_.data() + index_write_;
  }

  char *back() { return data_.data() + index_write_; }

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

 protected:
  std::vector<char> data_;
  size_t index_read_{0};
  size_t index_write_{0};
};

typedef flex_buffer buffer;

}  // namespace coring
#endif  // CORING_BUFFER_HPP
