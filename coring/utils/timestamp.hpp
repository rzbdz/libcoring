
#ifndef CORING_TIMESTAMP_HPP
#define CORING_TIMESTAMP_HPP
namespace coring {
class timestamp {
 public:
  typedef std::chrono::time_point<std::chrono::system_clock> raw_type;
  static raw_type now() { return std::chrono::system_clock::now(); };
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

 public:
  int year() { return static_cast<int>(ymd_data_.year()); }
  unsigned month() { return static_cast<unsigned>(ymd_data_.month()); }
  unsigned day() { return static_cast<unsigned>(ymd_data_.day()); }
  long hour() { return hms_data_.hours().count(); }
  long minute() { return hms_data_.minutes().count(); }
  long second() { return hms_data_.seconds().count(); }
  long microsecond() { return hms_data_.subseconds().count(); }

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
