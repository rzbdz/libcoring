///
/// only for logging module
/// while
#ifndef CORING_LOG_TIMESTAMP_HPP
#define CORING_LOG_TIMESTAMP_HPP
#include <chrono>
#include <cstdio>
#include <string>
#include "coring/utils/timestamp.hpp"
namespace coring {
class log_timestamp : public timestamp {
 private:
  // it's ok to init a constexpr or static const for it's completely
  // determined in compile time, differs to static variables
  static constexpr char TS_FMT_DATE[] = "%4d-%02u-%02u ";
  static constexpr char TS_FMT_BASE_TIME[] = "%02ld:%02ld:%02ld ";
  static constexpr char TS_FMT_MICRO_SEC[] = ".%06ld ";

 public:
  static constexpr int fmt_date_len = 11;
  static constexpr int fmt_base_time_len = 9;
  static constexpr int fmt_micro_second_len = 8;
  typedef log_timestamp ts_t;  // for possible refactoring use
  static constexpr int time_string_len = ts_t::fmt_date_len + ts_t::fmt_base_time_len + ts_t::fmt_micro_second_len;

 public:
  // I was trying to make a function that can build ts by subtraction result, but found useless for
  // this is a time point instead of a duration.
  // But I just found it stupid passing a log_timestamp btw threads especially 3 data differs lightly.
  explicit log_timestamp(const raw_type sys_timestamp) : timestamp{sys_timestamp} {}
  log_timestamp() noexcept : timestamp{} {};
  /// formatting:
 public:
  /// use ptr if as output, use ref if as const reference (lvalue)
  /// buf must larger than 4+2+2 + blanks
  ///
  /// \param buf
  /// \return return The buf + number of characters that would have been written if n had been sufficiently large, not
  /// counting the terminating null character.
  char *format_date_to(char *buf) {
    int w = ::snprintf(buf, fmt_date_len + 1, TS_FMT_DATE, year(), month(), day());
    return buf + w;
  }
  /// \param buf
  /// \return return The buf + number of characters that would have been written if n had been sufficiently large, not
  /// counting the terminating null character.
  char *format_base_time_to(char *buf) {
    int w = ::snprintf(buf, fmt_base_time_len + 1, TS_FMT_BASE_TIME, hour(), minute(), second());
    return buf + w;
  }
  /// \param buf
  /// \return return The buf + number of characters that would have been written if n had been sufficiently large, not
  /// counting the terminating null character.
  char *format_micro_to(char *buf) {
    int w = ::snprintf(buf, fmt_micro_second_len + 1, TS_FMT_MICRO_SEC, microsecond());
    return buf + w;
  }
  /// \param buf
  /// \return return The buf + number of characters that would have been written if n had been sufficiently large, not
  /// counting the terminating null character.
  char *format_time_to(char *buf) {
    auto n = format_base_time_to(buf);
    auto w = format_micro_to(n);
    return w;
  }
  /// \param buf
  /// \return return The buf + number of characters that would have been written if n had been sufficiently large, not
  /// counting the terminating null character.
  char *format_to(char *buf) {
    auto n = format_date_to(buf);
    auto w = format_time_to(n);
    return w;
  }
  std::string to_formatted_string() {
    char buf[40];
    format_to(buf);
    return buf;
  }

  /// operators:
 public:
  explicit operator std::string() { return to_formatted_string(); }

 private:
};
}  // namespace coring

#endif  // CORING_LOG_TIMESTAMP_HPP
