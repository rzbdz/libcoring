///
/// only for logging module
/// while
#ifndef CORING_TIMESTAMP_HPP
#define CORING_TIMESTAMP_HPP
#include <chrono>
#include <cstdio>
#include <string>
namespace coring {
class timestamp {
 private:
  static constexpr char TS_FMT_DATE[] = "%4d-%2u-%2u ";
  static constexpr char TS_FMT_BASE_TIME[] = "%02ld:%02ld:%02ld ";
  static constexpr char TS_FMT_MICRO_SEC[] = ".%06ld";

 public:
  static constexpr int fmt_date_len = sizeof TS_FMT_DATE;
  static constexpr int fmt_base_time_len = sizeof TS_FMT_BASE_TIME;
  static constexpr int fmt_micro_second_len = sizeof TS_FMT_MICRO_SEC;
  timestamp()
      : sys_timestamp_{std::chrono::system_clock::now()},
        ymd_data_{std::chrono::floor<std::chrono::days>(sys_timestamp_)},
        hms_data_{std::chrono::duration_cast<std::chrono::microseconds>(sys_timestamp_.time_since_epoch() - std::chrono::duration_cast<std::chrono::days>(sys_timestamp_.time_since_epoch()))} {}
  int year() { return static_cast<int>(ymd_data_.year()); }
  unsigned month() { return static_cast<unsigned>(ymd_data_.month()); }
  unsigned day() { return static_cast<unsigned>(ymd_data_.day()); }
  long hour() { return hms_data_.hours().count(); }
  long minute() { return hms_data_.minutes().count(); }
  long second() { return hms_data_.seconds().count(); }
  long microsecond() { return hms_data_.subseconds().count(); }

  bool compare_date(timestamp &rhs) {
    // TODO(pan): is there a performance problem?
    return std::chrono::floor<std::chrono::days>(sys_timestamp_) == std::chrono::floor<std::chrono::days>(rhs.sys_timestamp_);
  }
  bool compare_second(timestamp &rhs) {
    // TODO(pan):  is there a performance problem?
    return std::chrono::floor<std::chrono::seconds>(sys_timestamp_) == std::chrono::floor<std::chrono::seconds>(rhs.sys_timestamp_);
  }
  // use ptr if as output, use ref if as const reference (lvalue)
  // buf must larger than 4+2+2 + blanks
  char *format_date_to(char *buf) {
    int w = ::snprintf(buf, 50, TS_FMT_DATE, year(), month(), day());
    return buf + w;
  }
  char *format_base_time_to(char *buf) {
    int w = ::snprintf(buf, 50, TS_FMT_BASE_TIME, hour(), minute(), second());
    return buf + w;
  }
  char *format_micro_to(char *buf) {
    int w = ::snprintf(buf, 50, TS_FMT_MICRO_SEC, microsecond());
    return buf + w;
  }
  char *format_time_to(char *buf) {
    auto n = format_base_time_to(buf);
    auto w = format_micro_to(n);
    return w;
  }
  char *format_to(char *buf) {
    auto n = format_date_to(buf);
    auto w = format_time_to(n);
    return w;
  }

  std::string to_formatted_string() {
    char buf[40];
    auto n = format_to(buf);
    return buf;
  }

  explicit operator std::string() { return to_formatted_string(); }
  friend auto operator-(const timestamp &lhs, const timestamp &rhs) {
    //
    return std::chrono::duration_cast<std::chrono::microseconds>(lhs.sys_timestamp_.time_since_epoch() - rhs.sys_timestamp_.time_since_epoch());
  }

 private:
  timestamp(const std::chrono::time_point<std::chrono::system_clock>)
      : sys_timestamp_{std::chrono::system_clock::now()},
        ymd_data_{std::chrono::floor<std::chrono::days>(sys_timestamp_)},
        hms_data_{std::chrono::duration_cast<std::chrono::microseconds>(sys_timestamp_.time_since_epoch() - std::chrono::duration_cast<std::chrono::days>(sys_timestamp_.time_since_epoch()))} {}

  const std::chrono::time_point<std::chrono::system_clock> sys_timestamp_;
  const std::chrono::year_month_day ymd_data_;
  const std::chrono::hh_mm_ss<std::chrono::microseconds> hms_data_;
};
}  // namespace coring

#endif  // CORING_TIMESTAMP_HPP
