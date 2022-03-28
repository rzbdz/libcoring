
#ifndef CORING_FILE_DESCRIPTOR_HPP
#define CORING_FILE_DESCRIPTOR_HPP
namespace coring {

struct file_descriptor {
  int fd;
  operator int() { return fd; }
};

}  // namespace coring
#endif  // CORING_FILE_DESCRIPTOR_HPP
