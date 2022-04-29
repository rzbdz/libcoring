
#ifndef CORING_LOG_FILE_HPP
#define CORING_LOG_FILE_HPP
#include <cstdio>
#include <istream>
#include <fstream>
#include <utility>

#include "log_timestamp.hpp"
namespace coring {
// 40MB
class log_file {
 private:
  static const off_t k_file_roll_size;

 public:
  log_file(std::string name = "coring", off_t roll_size = k_file_roll_size);
  void roll_file();

  void append(char *data, std::streamsize len);
  void force_flush();

 private:
  std::string name_;
  off_t roll_size_;
  std::ofstream os_;
};
}  // namespace coring
#endif  // CORING_LOG_FILE_HPP
