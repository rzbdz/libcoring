
#include "buffer.hpp"
#include <cstdint>
namespace coring {
int buffer::capacity() { return 0; }
int buffer::can_write_bytes() { return 0; }
int buffer::remain_bytes() { return 0; }
template <typename IntType>
IntType buffer::get_a_int() {
  return nullptr;
}
std::string buffer::get_a_string() { return std::string(); }
template <typename IntType>
void buffer::put_a_int(IntType) {}
void buffer::put_a_string(std::string) {}
char *buffer::peek() { return nullptr; }
void buffer::pop(int len) {}
void buffer::clear() {}
int buffer::put(void *data, int len) { return 0; }
int buffer::read_to_file(int fd) { return 0; }
}  // namespace coring
