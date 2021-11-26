
#ifndef CORING_LOGGING_HPP
#define CORING_LOGGING_HPP
#include "../fmt/format.h"

// front end of logging module
namespace coring::detail {
class file_name_converter {
 public:
  template <int N>
  file_name_converter(const char (&arr)[N]) : data_(arr), size_(N - 1) {
    const char *slash = strrchr(data_, '/');  // builtin function
    if (slash) {
      data_ = slash + 1;
      size_ -= static_cast<int>(data_ - arr);
    }
  }

  explicit file_name_converter(const char *filename) : data_(filename) {
    const char *slash = strrchr(filename, '/');
    if (slash) {
      data_ = slash + 1;
    }
    size_ = static_cast<int>(strlen(data_));
  }

  const char *data_;
  int size_;
};
class logger_impl {};
}  // namespace coring::detail
namespace coring {

// use in anonymous object
class logger {
  // compile time calculation of basename of source file
 public:
  typedef void (*async_interface_t)(const char *line, int len);
  enum log_level {
    TRACE,
    DEBUG,
    INFO,
    WARN,
    ERROR,
    FATAL,
    LEVELS_CNT,
  };
  using file_name_t = detail::file_name_converter;
  // use in anonymous object
  logger(file_name_t file, int line, log_level level = INFO) {}
  ~logger() {}

 private:
  detail::logger_impl impl;
  static async_interface_t output;
};

}  // namespace coring

#endif  // CORING_LOGGING_HPP
