
#ifndef CORING_CONST_BUFFER_HPP
#define CORING_CONST_BUFFER_HPP
#include <cstddef>
#include <string>
#include <cassert>
#include <cstring>
#include "buffer_base.hpp"
#include "coring/net/endian.hpp"
namespace coring {
class const_buffer : public detail::buffer_base {
 public:
  const_buffer(const char *s, size_t len) : buffer_base{0, len}, start_{s} {}
  template <size_t N>
  const_buffer(const char (&s)[N]) : buffer_base{0, N - 1}, start_{s} {
    // truncate the '\0' end flag.
  }
  const char *front() const { return (start_ + index_read_); }
  std::string pop_string(size_t len) {
    if (len > readable()) {
      len = readable();
    }
    std::string ret(front(), len);
    pop_front(len);
    return ret;
  }

  template <typename IntType>
  IntType pop_int() {
    assert(readable() >= sizeof(IntType));
    IntType ret = 0;
    ::memcpy(&ret, front(), sizeof(IntType));
    pop_front(sizeof(IntType));
    return coring::net::network_to_host(ret);
  }

 private:
  const char *start_;
};
}  // namespace coring
#endif  // CORING_CONST_BUFFER_HPP
