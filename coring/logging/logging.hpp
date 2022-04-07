/// front end of logging module
#ifndef CORING_LOGGING_HPP
#define CORING_LOGGING_HPP

#include "log_timestamp.hpp"
#include "logging-inl.hpp"
#include <chrono>
#include <vector>

// exposed for testing reason
struct TEST;
namespace coring::detail {
class async_logger;
}
namespace coring {
enum log_level {
  TRACE = 0,
  DEBUG = 1,
  INFO = 2,
  WARN = 3,
  ERROR = 4,
  FATAL = 5,
  LOG_LEVEL_CNT,
};
// bugs when use constexpr!
extern log_level LOG_LEVEL;

inline void set_log_level(log_level l) { LOG_LEVEL = l; }

inline bool SHOULD_EMIT(log_level exp) {
  if (exp >= LOG_LEVEL) {
    return true;
  } else {
    return false;
  }
}

/// This class is used to implicit convert a file name
/// to it's basename in compile time.
class file_name_converter {
 public:
  template <int N>
  file_name_converter(const char (&arr)[N]) : data_(arr), size_(N - 1) {  // NOLINT(google-explicit-constructor)
    const char *slash = strrchr(data_, '/');                              // builtin function
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
  file_name_converter() = default;

  const char *data_;
  int size_;
};

using file_name_t = file_name_converter;
}  // namespace coring
namespace coring::detail {

struct log_entry {
 public:
  static constexpr int pid_string_len = 6;
  // compiler optimizes it
  log_entry(file_name_t file, int line, log_level lv, const char *pid);
  log_entry() = default;
  friend TEST;
  file_name_t file_;
  int line_;
  log_level lv_;
  // log_timestamp ts_;
  log_timestamp::raw_type ts_;
  const char *pid_string_;
};
}  // namespace coring::detail

namespace coring {
// use in anonymous object
class logger {
  // compile time calculation of basename of source file
 public:
  // use in anonymous object
  logger(file_name_t file, int line, log_level level = INFO);

  // trivial
  ~logger() = default;

  typedef std::back_insert_iterator<std::vector<char>> output_iterator_t;
  typedef void (*submit_interface)(std::function<void(output_iterator_t)> &&, detail::log_entry &);

  template <typename... Args>
  void log(fmt::format_string<Args...> &&fmt, Args &&...args) {
    submitter_(detail::make_log_task(std::move(fmt), args...), log_entry_);
  }

  static void register_submitter(submit_interface s) { submitter_ = s; }

 private:
  friend TEST;
  friend detail::async_logger;
  detail::log_entry log_entry_;

  const static char *log_level_map_[LOG_LEVEL_CNT];
  static submit_interface submitter_;
};
#define _LOG_GEN_(level, fmt, ...)                                          \
  if (SHOULD_EMIT(level)) {                                                 \
    coring::logger(__FILE__, __LINE__, level).log(fmt "\n", ##__VA_ARGS__); \
  } else

// macro notes:
// '#' is for cstring, '##' is for symbol
// notice that if ##__VA_ARGS__ is used,it must follow a ,
#define LOG_TRACE(fmt, ...) _LOG_GEN_(coring::TRACE, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) _LOG_GEN_(coring::DEBUG, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) _LOG_GEN_(coring::INFO, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) _LOG_GEN_(coring::WARN, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) _LOG_GEN_(coring::ERROR, fmt, ##__VA_ARGS__)
#define LOG_FATAL(fmt, ...) _LOG_GEN_(coring::FATAL, fmt, ##__VA_ARGS__)
}  // namespace coring

#endif  // CORING_LOGGING_HPP