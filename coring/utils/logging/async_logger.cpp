#include "async_logger.hpp"
#include "timestamp.hpp"
#include "../debug.hpp"

namespace coring::detail {

thread_local detail::log_ring_ptr local_log_ring_ptr;

async_logger *single = nullptr;

// thread_local log_ring_ptr local_log_ring_ptr;
void async_submitter(std::function<void(logger::output_iterator_t)> &&f, log_entry &e) {
  single->append(std::forward<std::function<void(logger::output_iterator_t)>>(f), (e));
}
async_logger::async_logger(std::string file_name, off_t roll_size, int flush_interval)
    : file_name_{std::move(file_name)}, roll_size_{roll_size}, flush_interval_{flush_interval} {
  LOG_DEBUG_RAW("init a async logger");
  time_buffer_[timestamp::time_string_len - 1] = '\0';
  writing_buffer_ = std::make_unique<log_buffer_t>();
  writing_buffer_->reserve(k_max_buffer);
  log_rings_.reserve(4);
  not_empty_rings_.reserve(4);
  update_datetime();
  single = this;
  logger::register_submitter(async_submitter);
}
void async_logger::poll(bool force_flush) {
  LOG_DEBUG_RAW("do_polling");
  std::vector<log_ring_entry_t> to_handle;
  // not flushed to file in this wake up
  flushed_ = false;
  {
    std::lock_guard lk(mutex_);
    LOG_DEBUG_RAW("lock acquired!");
    if (!log_rings_.empty()) {
      LOG_DEBUG_RAW("rings is not empty! %lu", log_rings_.size());
      // no contention except the very first logging of every thread
      for (auto &ring : log_rings_) {
        ssize_t have = static_cast<ssize_t>(ring->size());
        LOG_DEBUG_RAW("ring have size: %lu", have);
        while (have != 0) {
          --have;
          auto p = ring->front();
          assert(p);
          to_handle.emplace_back(std::move(*(p)));
          ring->pop();
        }
      }
    }
    LOG_DEBUG_RAW("release lock!");
  }
  // go to sleep
  assert(writing_buffer_.get());
  if (to_handle.empty() && writing_buffer_->size() <= 0) {
    LOG_DEBUG_RAW("go to sleep");
    return;
  }
  LOG_DEBUG_RAW("writingbuffer is bigger than 0");
  for (auto &h : to_handle) {
    LOG_DEBUG_RAW("do have handle!");
    auto [f, e] = h;
    update_datetime(timestamp(e.ts_));
    write_prefix(e);
    auto out = std::back_inserter(*writing_buffer_);
    f(out);
    write_filename(e);
    fmt::format_to(out, ":{}\n", e.line_);
    LOG_DEBUG_RAW("see a log:");
    //    for (auto &c : *writing_buffer_) {
    //      std::cout << c;
    //    }
  }
  assert(writing_buffer_.get());
  if ((force_flush && writing_buffer_->size() > 0) ||
      writing_buffer_->size() >= k_max_buffer - suggested_single_log_max_len) {
    // flush to file
    LOG_DEBUG_RAW("ready to flush a file %lu", writing_buffer_->size());
    f_.append(writing_buffer_->data(), static_cast<ptrdiff_t>(writing_buffer_->size()));
    writing_buffer_->clear();
    flushed_ = true;
  }
  assert(writing_buffer_.get());
  //  f_.append(writing_buffer_->data(), static_cast<ptrdiff_t>(writing_buffer_->size()));
}
void async_logger::busy_poll() {
  while (running_) {
    if (log_rings_.empty()) {
      std::unique_lock lk(mutex_);
      cond_.wait_for(lk, std::chrono::seconds(flush_interval_));
    }
    // no contention except the very first logging of every thread
    {
      std::lock_guard lk(mutex_);
      for (auto &ring : log_rings_) {
        auto h = *ring->front();
        auto [f, e] = h;
        ring->pop();
        update_datetime(timestamp(e.ts_));
        write_prefix(e);
        auto out = std::back_inserter(*writing_buffer_);
        f(out);
        write_filename(e);
        fmt::format_to(out, ":{}\n", e.line_);
      }
    }  // leave room for new thread to come in .
    if (writing_buffer_->size() >= k_max_buffer - suggested_single_log_max_len) {
      // flush to file
      f_.append(writing_buffer_->data(), static_cast<ptrdiff_t>(writing_buffer_->size()));
      flushed_ = true;
    }
  }
  f_.append(writing_buffer_->data(), static_cast<ptrdiff_t>(writing_buffer_->size()));
}

}  // namespace coring::detail
