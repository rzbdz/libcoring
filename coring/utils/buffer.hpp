
#ifndef CORING_BUFFER_HPP
#define CORING_BUFFER_HPP

#include <vector>
#include <string>
namespace coring {
class buffer {
 public:
  // meta
  int capacity();
  int can_write_bytes();
  int remain_bytes();

  // get int/string from data
  template <typename IntType>
  IntType get_a_int();
  std::string get_a_string();

  // put int/string to data
  template <typename IntType>
  void put_a_int(IntType);
  void put_a_string(std::string);

  // read  directly
  char *peek();
  void pop(int len);
  void clear();

  // write directly
  int put(void *data, int len);

  // file operation
  int read_to_file(int fd);
  // void put_from_file(int fd, int offset, int len);

  // TODO: more extension
  // prepend data
  // shrink

 private:
  std::vector<char> data_;
  size_t pos_consumer_;
  size_t pos_producer_;
};
}  // namespace coring
#endif  // CORING_BUFFER_HPP
