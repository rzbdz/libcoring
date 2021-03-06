/// Async_logger is the backend of async logging module.
///
/// Note that this implementation would be in low latency mode instead of high throughput mode.
/// - background-thread formatting == low latency + low throughput(for backend being the bottleneck)
/// - front-end formatting == average high latency + high throughput (using double buffer to flush to file)
///
/// if we do want both best performance, the only approach would be offline formatting, which means only write dynamic
/// data and type information to file(in binary), and use offline tools to reconstruct the complete logs
/// @see: https://www.usenix.org/system/files/conference/atc18/atc18-yang.pdf
///
/// after some profiling, found that the main bottleneck would be the log_timestamp system,
/// more detail see commit log, it's sad that I didn't wrote down more comments here in codes before writing commit log.

#ifndef CORING_ETC_LOGGING_LOGGER_H
#define CORING_ETC_LOGGING_LOGGER_H

#include <mutex>
#include <condition_variable>
#include <latch>
#include <utility>
#include <iostream>
#include <vector>
#include <deque>
#include <string>
#include <thread>
#include <functional>
#include <csignal>

#include "coring/detail/logging/fmt/format.h"
#include "coring/detail/noncopyable.hpp"
#include "coring/detail/logging/spsc_ring.hpp"
#include "coring/detail/logging/log_file.hpp"

#include "logging.hpp"
namespace coring ::detail {

typedef std::function<void(logger::output_iterator_t)> logging_func_t;
typedef std::pair<logging_func_t, log_entry> log_ring_entry_t;
typedef spsc_ring<log_ring_entry_t> ring_t;
typedef std::shared_ptr<ring_t> log_ring_ptr;

extern thread_local detail::log_ring_ptr local_log_ring_ptr;

class async_logger : noncopyable {
 public:
  typedef std::back_insert_iterator<std::vector<char>> output_iterator_t;
  typedef std::vector<char> log_buffer_t;
  typedef std::unique_ptr<log_buffer_t> log_buffer_ptr;
  // TODO: use a thread_local pool to dispatch buffer per pool
  static const size_t k_max_buffer;
  static const size_t suggested_single_log_max_len;
  static const size_t ring_buffer_size;

 public:
  explicit async_logger(std::string file_name = "", int flush_interval = 3);

  ~async_logger();

 public:
  void append(std::function<void(output_iterator_t)> &&f, detail::log_entry &e);

  void run();
  inline void start() { run(); }
  inline void enable() { run(); }

  void block_sigint_then_run();

  void poll();
  void logging_loop();

  void stop();

  void signal() { signal_ = true; }

 private:
  const int flush_interval_;
  std::mutex mutex_{};
  std::condition_variable cond_{};
  std::latch count_down_latch_{1};
  // bool running_{false};
  std::stop_source stop_source_{};
  bool signal_{false};
  bool force_flush_{false};
  std::jthread thread_{};

 private:
  std::vector<log_ring_ptr> log_rings_;

 private:
  log_buffer_ptr writing_buffer_;

 private:
  // log_timestamp won't be thread local for difficult management
  // typedef std::shared_ptr<log_timestamp> timestamp_ptr;
  log_timestamp last_time_{};
  // fuck the '\0' appended by sprintf.... fuck him fuck me;
  char time_buffer_[log_timestamp::time_string_len + 1]{};

  void update_datetime() {
    char *end = last_time_.format_to(time_buffer_);
    assert_eq(end, time_buffer_ + log_timestamp::time_string_len);
  }
  void update_datetime_time() {
    char *end = last_time_.format_time_to(time_buffer_ + log_timestamp::fmt_date_len);
    assert_eq(end, time_buffer_ + log_timestamp::time_string_len);
  }
  void update_datetime_micro() {
    char *end =
        last_time_.format_micro_to(time_buffer_ + log_timestamp::fmt_date_len + log_timestamp::fmt_base_time_len);
    assert_eq(end, time_buffer_ + log_timestamp::time_string_len);
  }
  void update_datetime(log_timestamp &&ts) {
    std::swap(last_time_, ts);
    if (ts.same_date(last_time_)) {
      if (ts.same_second(last_time_)) {
        update_datetime_micro();
      } else {
        update_datetime_time();
      }
    } else {
      update_datetime();
    }
  }

  // These indirect writing and copying should have performance problem
  // but not bottleneck.
  // No more optimization would be made since the most serious
  // bottleneck (99%) would be the log_timestamp system.
 private:
  void write_datetime() {
    writing_buffer_->insert(writing_buffer_->end(), time_buffer_, time_buffer_ + log_timestamp::time_string_len);
  }
  void write_pid(log_entry &e) {
    writing_buffer_->insert(writing_buffer_->end(), e.pid_string_,
                            e.pid_string_ + coring::detail::log_entry::pid_string_len);
  }
  void write_level(log_entry &e) {
    writing_buffer_->insert(writing_buffer_->end(), logger::log_level_map_[e.lv_], logger::log_level_map_[e.lv_] + 5);
  }
  void write_prefix(log_entry &e) {
    write_datetime();
    write_pid(e);
    write_level(e);
  }
  void write_space() { writing_buffer_->insert(writing_buffer_->end(), ' '); }
  void write_filename(log_entry &e) {
    writing_buffer_->insert(writing_buffer_->end(), e.file_.data_, e.file_.data_ + e.file_.size_);
  }

 private:
  log_file output_file_;
};

}  // namespace coring::detail
namespace coring {
typedef detail::async_logger async_logger;
}
#endif  // CORING_ETC_LOGGING_LOGGER_H