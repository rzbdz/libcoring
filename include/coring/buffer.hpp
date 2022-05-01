#ifndef CORING_BUFFER_HPP
#define CORING_BUFFER_HPP

#include "coring/coring_config.hpp"
#include "coring/detail/noncopyable.hpp"
#include "coring/detail/debug.hpp"
#include "coring/endian.hpp"
#include <vector>
#include <string>
#include <cassert>
#include <cstring>
#include <sys/uio.h>
#include <iostream>

namespace coring {

namespace detail {
// 4 bytes would be a waste...
constexpr static const char kCRLF[] = "\r\n\r\n";

class buffer_view {
 public:
  char *data_;
  size_t len_;
  [[nodiscard]] inline char *data() { return data_; }              // NOLINT
  [[nodiscard]] inline const char *data() const { return data_; }  // NOLINT
  [[nodiscard]] inline size_t capacity() const { return len_; }
  [[nodiscard]] inline size_t size() const { return len_; }
};

template <typename Container>
class buffer_base : noncopyable {
  friend CORING_TEST_CLASS;

 public:
  static constexpr size_t default_size = BUFFER_DEFAULT_SIZE;

 public:
  explicit buffer_base(Container &&data) : container_(std::move(data)) {}
  buffer_base(buffer_base &&rhs) noexcept
      : container_(std::move(rhs.container_)), index_read_(rhs.index_read_), index_write_(rhs.index_write_) {}
  [[nodiscard]] inline char *data() { return container_.data(); }              // NOLINT
  [[nodiscard]] inline const char *data() const { return container_.data(); }  // NOLINT

  virtual ~buffer_base() = default;

 public:
  [[nodiscard]] size_t readable() const { return index_write_ - index_read_; }
  [[nodiscard]] size_t capacity() const { return container_.capacity(); }
  [[nodiscard]] inline size_t size() const { return container_.size(); }

  [[nodiscard]] size_t writable() const { return container_.size() - index_write_; }

 public:
  void clear() { index_read_ = index_write_ = 0; }

  void has_read(size_t len) {
    assert(len <= readable());
    if (len == readable()) {
      //      LDR("pop_front %lu, but is larger, clear all", len);
      clear();
    } else {
      index_read_ += len;
      //      LDR("pop_front %lu, now: con: %lu, pro: %lu", len, index_read_, index_write_);
    }
  }

  [[nodiscard]] const char *front() const {
    //    LDR("front, return const cannot be modifyied");
    return (container_.data() + index_read_);
  }

  [[nodiscard]] char *front() {
    //    LDR("front, return const cannot be modifyied");
    return (container_.data() + index_read_);
  }

  std::string pop_string(size_t len) {
    //    LDR("pop_front a string: %lu", len);
    if (len > readable()) {
      //      LDR("ask for too long!");
      len = readable();
    }
    std::string ret(front(), len);
    has_read(len);
    return ret;
  }

  template <typename IntType>
  IntType pop_int() {
    //    LDR("ask pop %lu bytes", sizeof(IntType));
    assert(readable() >= sizeof(IntType));
    IntType ret = 0;
    ::memcpy(&ret, front(), sizeof(IntType));
    has_read(sizeof(IntType));
    return coring::net::network_to_host(ret);
  }

  [[nodiscard]] const char *back() const {
    //    LDR("back, dangerous, %lu", index_write_);
    return container_.data() + index_write_;
  }

  char *back() { return container_.data() + index_write_; }

  void has_written(size_t len) {
    //    LDR("increase length by %lu, ori:", len);
    index_write_ += len;
    //    LDR("now:");
  }

  /// As named
  /// \return the place of first cr (\r)
  [[nodiscard]] const char *find(char ch) const {
    const char *crlf = memchr(front(), ch, readable());
    return crlf == back() ? nullptr : crlf;
  }

  /// As named
  /// \return the place of first cr (\r)
  [[nodiscard]] const char *find_crlf() const {
    const char *crlf = std::search(front(), back(), kCRLF, kCRLF + 2);
    return crlf == back() ? nullptr : crlf;
  }

  /// as named
  /// \param start
  /// \return the place of first cr (\r)
  [[nodiscard]] const char *find_crlf(const char *start) const {
    const char *crlf = std::search(start, back(), kCRLF, kCRLF + 2);
    return crlf == back() ? nullptr : crlf;
  }

  /// For http header separator (RFC 2616 s4.1)
  /// \return the place of first cr (\r)
  [[nodiscard]] const char *find_2crlf() const {
    const char *crlf = std::search(front(), back(), kCRLF, kCRLF + 4);
    return crlf == back() ? nullptr : crlf;
  }

  /// For http header separator (RFC 2616 s4.1)
  /// \param start
  /// \return the place of first cr (\r)
  [[nodiscard]] const char *find_2crlf(const char *start) const {
    const char *crlf = std::search(start, back(), kCRLF, kCRLF + 4);
    return crlf == back() ? nullptr : crlf;
  }

  /// find a '\n'
  /// \return the place of first lf
  [[nodiscard]] const char *find_eol() const {
    const void *eol = memchr(front(), '\n', readable());
    return static_cast<const char *>(eol);
  }

  /// find a '\n'
  /// \return the place of first lf
  [[nodiscard]] const char *findEOL(const char *start) const {
    const void *eol = memchr(start, '\n', back() - start);
    return static_cast<const char *>(eol);
  }

 protected:
  Container container_;
  size_t index_read_{0};
  size_t index_write_{0};
};
}  // namespace detail

/// for char array, just the data to send
class fixed_buffer : public detail::buffer_base<detail::buffer_view> {
  typedef detail::buffer_base<detail::buffer_view> FatherType;

 public:
  fixed_buffer(char *s, size_t len) : FatherType{{s, len}} {}
  template <size_t N>
  explicit fixed_buffer(char (&s)[N]) : FatherType{{s, N - 1}} {
    // truncate the '\0' end flag.
  }

  void make_room(size_t want) {
    if (index_read_ > 0) {
      size_t now_have = readable();
      ::memmove(container_.data() + index_read_, front(), now_have);
      index_read_ = 0;
      index_write_ = now_have;
      //      LDR("make room: move readable %lu", want);
    }
    if (writable() < want) {
      throw std::runtime_error("cannot make room for fixed buffer");
      // container_.resize(index_write_ + want);
    }
  }

  /// it won't call make_room internal
  void emplace_back(char ch) {
    //    LDR("in push_back: ask for %lu, first byte in src %d", len, (int)(*reinterpret_cast<const char *>(src)));
    // make_room(1);
    char *dst = const_cast<char *>(back());
    //    LDR("back: 0x%lx", (size_t)dst);
    // ::memcpy(dst, src, len);
    *dst = ch;
    //    LDR("after copy, first byte in dst: %d", (int)(*dst));
    has_written(1);
  }

  void push_back(char ch) {
    make_room(1);
    emplace_back(ch);
  }

  /// You don't need to make room.
  void emplace_back(const void *src, size_t len) {
    //    LDR("in push_back: ask for %lu, first byte in src %d", len, (int)(*reinterpret_cast<const char *>(src)));
    make_room(len);
    char *dst = const_cast<char *>(back());
    //    LDR("back: 0x%lx", (size_t)dst);
    ::memcpy(dst, src, len);
    //    LDR("after copy, first byte in dst: %d", (int)(*dst));
    has_written(len);
  }

  // put int/string to data
  template <typename IntType>
  void emplace_back_int(IntType to_put) {
    //    LDR("push_back a int %lu: %lu bytes", (size_t)to_put, sizeof(to_put));
    to_put = coring::net::host_to_network(to_put);
    //    LDR("to put is now : %lu", (size_t)to_put);
    emplace_back(&to_put, sizeof(IntType));
  }

  void emplace_back_string(std::string str) {
    size_t len = str.size();
    //    LDR("push back a string: %s, %lu", str.data(), len);
    emplace_back(str.data(), len);
  }
};

class flex_buffer : public detail::buffer_base<std::vector<char>> {
  typedef detail::buffer_base<std::vector<char>> FatherType;

 public:
  typedef char value_type;
  explicit flex_buffer(int init_size = BUFFER_DEFAULT_SIZE) : FatherType{std::vector<char>(init_size)} {}

  void make_room(size_t want) {
    //    LDR("make room: %lu", want);
    if (index_read_ > 0) {
      size_t now_have = readable();
      ::memmove(container_.data() + index_read_, front(), now_have);
      index_read_ = 0;
      index_write_ = now_have;
      //      LDR("make room: move readable %lu", want);
    }
    if (writable() < want) {
      container_.resize(index_write_ + want);
    }
  }
  ~flex_buffer() override = default;

  /// it won't call make_room internal
  void emplace_back(char ch) {
    //    LDR("in push_back: ask for %lu, first byte in src %d", len, (int)(*reinterpret_cast<const char *>(src)));
    // make_room(1);
    char *dst = const_cast<char *>(back());
    //    LDR("back: 0x%lx", (size_t)dst);
    // ::memcpy(dst, src, len);
    *dst = ch;
    //    LDR("after copy, first byte in dst: %d", (int)(*dst));
    has_written(1);
  }

  void push_back(char ch) {
    make_room(1);
    emplace_back(ch);
  }

  /// You don't need to make room.
  void emplace_back(const void *src, size_t len) {
    //    LDR("in push_back: ask for %lu, first byte in src %d", len, (int)(*reinterpret_cast<const char *>(src)));
    make_room(len);
    char *dst = const_cast<char *>(back());
    //    LDR("back: 0x%lx", (size_t)dst);
    ::memcpy(dst, src, len);
    //    LDR("after copy, first byte in dst: %d", (int)(*dst));
    has_written(len);
  }

  // put int/string to data
  template <typename IntType>
  void emplace_back_int(IntType to_put) {
    //    LDR("push_back a int %lu: %lu bytes", (size_t)to_put, sizeof(to_put));
    to_put = coring::net::host_to_network(to_put);
    //    LDR("to put is now : %lu", (size_t)to_put);
    emplace_back(&to_put, sizeof(IntType));
  }

  void emplace_back_string(std::string str) {
    size_t len = str.size();
    //    LDR("push back a string: %s, %lu", str.data(), len);
    emplace_back(str.data(), len);
  }
};

typedef flex_buffer buffer;

}  // namespace coring
#endif  // CORING_BUFFER_HPP
