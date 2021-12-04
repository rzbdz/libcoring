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
/// after some profiling, found that the main bottleneck would be the timestamp system,
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

#include "../fmt/format.h"
#include "../noncopyable.hpp"
#include "logging.hpp"
#include "spsc_ring.hpp"
#include "log_file.hpp"
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
  // but that brings too much coupling between threading and logger
  static constexpr size_t k_max_buffer = 1000 * 4000;
  static constexpr size_t suggested_single_log_max_len = 500;
  static constexpr size_t ring_buffer_size = 1024;

 public:
  explicit async_logger(std::string file_name = "", off_t roll_size = k_file_roll_size, int flush_interval = 3);

  ~async_logger() {
    stop();
    output_file_.force_flush();
  }

 public:
  void append(std::function<void(output_iterator_t)> &&f, detail::log_entry &e) {
    if (__glibc_unlikely(local_log_ring_ptr == nullptr)) {
      std::lock_guard lk(mutex_);
      local_log_ring_ptr = std::make_shared<ring_t>(ring_buffer_size);
      log_rings_.emplace_back(local_log_ring_ptr);
      signal();
    }
    local_log_ring_ptr->emplace_back(std::move(f), e);
    // wake up consumer. (if needed)
    cond_.notify_one();
  }

  void run() {
    if (__glibc_unlikely(thread_.joinable())) {
      stop();
    }
    running_ = true;
    thread_ = std::jthread{[this] { logging_loop(); }};
    // make sure it 's running then return to caller
    count_down_latch_.wait();
  }

  void poll();
  void logging_loop() {
    count_down_latch_.count_down();
    while (running_) {
      poll();
      std::unique_lock lk(mutex_);
      cond_.wait_for(lk, std::chrono::seconds(flush_interval_));
    }
    force_flush_ = true;
    poll();
    output_file_.force_flush();
  }

  void stop() {
    running_ = false;
    force_flush_ = true;
    cond_.notify_all();
    // this is stupid because jthread member is
    // sometime destruct after writing_buffer(impl defined),
    // and no solution unless put jthread
    // to main thread to have a better lifetime.
    if (thread_.joinable()) {
      thread_.join();
    }
  }

  void signal() { signal_ = true; }

 private:
  const int flush_interval_;
  std::mutex mutex_{};
  std::condition_variable cond_{};
  std::latch count_down_latch_{1};
  bool running_{false};
  bool signal_{false};
  bool force_flush_{false};
  std::jthread thread_{};

 private:
  std::vector<log_ring_ptr> log_rings_;

 private:
  log_buffer_ptr writing_buffer_;

 private:
  // timestamp won't be thread local for difficult management
  // typedef std::shared_ptr<timestamp> timestamp_ptr;
  timestamp last_time_{};
  char time_buffer_[timestamp::time_string_len]{};

  void update_datetime() {
    char *end = last_time_.format_to(time_buffer_);
    assert_eq(end, time_buffer_ + timestamp::time_string_len);
  }
  void update_datetime_time() {
    char *end = last_time_.format_time_to(time_buffer_ + timestamp::fmt_date_len);
    assert_eq(end, time_buffer_ + timestamp::time_string_len);
  }
  void update_datetime_micro() {
    char *end = last_time_.format_micro_to(time_buffer_ + timestamp::fmt_date_len + timestamp::fmt_base_time_len);
    assert_eq(end, time_buffer_ + timestamp::time_string_len);
  }
  void update_datetime(timestamp &&ts) {
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
  // bottleneck (99%) would be the timestamp system.
 private:
  void write_datetime() {
    writing_buffer_->insert(writing_buffer_->end(), time_buffer_, time_buffer_ + timestamp::time_string_len);
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