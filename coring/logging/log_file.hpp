
#ifndef CORING_LOG_FILE_HPP
#define CORING_LOG_FILE_HPP
#include <cstdio>
#include <istream>
#include <fstream>
#include <utility>
#include "log_timestamp.hpp"
namespace coring {
// 40MB
constexpr off_t k_file_roll_size = 40 * 1024 * 1024;
class log_file {
 public:
  log_file(std::string name = "coring", off_t roll_size = k_file_roll_size)
      : name_{std::move(name)}, roll_size_{roll_size} {
    roll_file();
  }
  void roll_file() {
    log_timestamp ts;
    char buf[40];
    char *ret = ts.format_date_to(buf) - 1;
    ret[0] = '.';
    ret[1] = 'l';
    ret[2] = 'o';
    ret[3] = 'g';
    ret[4] = '\0';
    name_.append(buf);
    os_ = std::ofstream{name_, std::ios::app | std::ios::out};
  }

  void append(char *data, std::streamsize len) {
    // Getting the current write position.
    if (os_.tellp() >= roll_size_) {
      os_.flush();
      roll_file();
    }
    os_.write(data, len);
  }
  void force_flush() { os_.flush(); }

 private:
  std::string name_;
  off_t roll_size_;
  std::ofstream os_;
};
}  // namespace coring
class log_stdout {
 public:
  log_stdout(const std::string &name = "coring", off_t roll_size = 0) {
    char buf[] = "Async Logger started, using stdout as output, log name: ";
    fwrite(buf, 1, sizeof(buf), stdout);
    fwrite(name.c_str(), 1, name.size(), stdout);
    fwrite("\n", 1, 1, stdout);
  }
  void roll_file() {
    // do nothing...
  }
  void append(char *data, std::streamsize len) { fwrite(data, 1, len, stdout); }
  void force_flush() { fflush(stdout); }
};
#endif  // CORING_LOG_FILE_HPP
