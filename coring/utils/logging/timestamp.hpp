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
  // it's ok to init a constexpr or static const for it's completely
  // determined in compile time, differs to static variables
  static constexpr char TS_FMT_DATE[] = "%4d-%02u-%02u ";
  static constexpr char TS_FMT_BASE_TIME[] = "%02ld:%02ld:%02ld ";
  static constexpr char TS_FMT_MICRO_SEC[] = ".%06ld ";

 public:
  typedef std::chrono::time_point<std::chrono::system_clock> raw_type;
  static raw_type now() { return std::chrono::system_clock::now(); };
  static constexpr int fmt_date_len = 11;
  static constexpr int fmt_base_time_len = 9;
  static constexpr int fmt_micro_second_len = 8;
  typedef timestamp ts_t;  // for possible refactoring use
  static constexpr int time_string_len = ts_t::fmt_date_len + ts_t::fmt_base_time_len + ts_t::fmt_micro_second_len;

 public:
  // I was trying to make a function that can build ts by subtraction result, but found useless for
  // this is a time point instead of a duration.
  // But I just found it stupid passing a timestamp btw threads especially 3 data differs lightly.
  explicit timestamp(const raw_type sys_timestamp)
      : sys_timestamp_{sys_timestamp},
        ymd_data_{std::chrono::floor<std::chrono::days>(sys_timestamp_)},
        hms_data_{std::chrono::duration_cast<std::chrono::microseconds>(
            sys_timestamp_.time_since_epoch() -
            std::chrono::duration_cast<std::chrono::days>(sys_timestamp_.time_since_epoch()))} {}
  timestamp() noexcept
      : sys_timestamp_{std::chrono::system_clock::now()},
        ymd_data_{std::chrono::floor<std::chrono::days>(sys_timestamp_)},
        hms_data_{std::chrono::duration_cast<std::chrono::microseconds>(
            sys_timestamp_.time_since_epoch() -
            std::chrono::duration_cast<std::chrono::days>(sys_timestamp_.time_since_epoch()))} {}

  // they are all pod
  //  timestamp(timestamp &&ts) {
  //    sys_timestamp_ = std::move(ts.sys_timestamp_);
  //    ymd_data_ = std::move(ts.ymd_data_);
  //    hms_data_ = std::move(ts.hms_data_);
  //  }
  //  timestamp &operator=(timestamp &&ts) {
  //    sys_timestamp_ = std::move(ts.sys_timestamp_);
  //    ymd_data_ = std::move(ts.ymd_data_);
  //    hms_data_ = std::move(ts.hms_data_);
  //  }

  // data:
 public:
  int year() { return static_cast<int>(ymd_data_.year()); }
  unsigned month() { return static_cast<unsigned>(ymd_data_.month()); }
  unsigned day() { return static_cast<unsigned>(ymd_data_.day()); }
  long hour() { return hms_data_.hours().count(); }
  long minute() { return hms_data_.minutes().count(); }
  long second() { return hms_data_.seconds().count(); }
  long microsecond() { return hms_data_.subseconds().count(); }

  /// cannot use operator overloads for different precision
 public:
  bool same_date(timestamp &rhs) {
    // TODO: is there a performance problem?
    return std::chrono::floor<std::chrono::days>(sys_timestamp_) ==
           std::chrono::floor<std::chrono::days>(rhs.sys_timestamp_);
  }
  bool same_second(timestamp &rhs) {
    // TODO:  is there a performance problem?
    return std::chrono::floor<std::chrono::seconds>(sys_timestamp_) ==
           std::chrono::floor<std::chrono::seconds>(rhs.sys_timestamp_);
  }

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
  friend auto operator-(const timestamp &lhs, const timestamp &rhs) {
    return std::chrono::duration_cast<std::chrono::microseconds>(lhs.sys_timestamp_.time_since_epoch() -
                                                                 rhs.sys_timestamp_.time_since_epoch());
  }

 private:
  raw_type sys_timestamp_;
  std::chrono::year_month_day ymd_data_;
  std::chrono::hh_mm_ss<std::chrono::microseconds> hms_data_;
};
}  // namespace coring

#endif  // CORING_TIMESTAMP_HPP
