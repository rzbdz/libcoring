#include "async_logger.hpp"
#include "timestamp.hpp"
#include "coring/utils/debug.hpp"

namespace coring::detail {

thread_local detail::log_ring_ptr local_log_ring_ptr;

async_logger *as_logger_single = nullptr;

// thread_local log_ring_ptr local_log_ring_ptr;
void async_submitter(std::function<void(logger::output_iterator_t)> &&f, log_entry &e) {
  as_logger_single->append(std::forward<std::function<void(logger::output_iterator_t)>>(f), (e));
}
async_logger::async_logger(std::string file_name, off_t roll_size, int flush_interval)
    : flush_interval_{flush_interval}, output_file_{std::move(file_name), roll_size} {
  // init the time_buffer first, at lease the date would be reused.
  update_datetime();
  time_buffer_[timestamp::time_string_len - 1] = '\0';
  // setup char buffer, performance problem do exist, but not as bottleneck.
  writing_buffer_ = std::make_unique<log_buffer_t>();
  // init an almost 4mb char buffer, not 4k aligned to avoid cache sharing
  writing_buffer_->reserve(k_max_buffer);
  // actually should reserve more than 4
  // C=CPU cores, P=CPU busy time / running time,  0<P<=1, T=thread, T=C/P
  log_rings_.reserve(4);
  as_logger_single = this;
  logger::register_submitter(async_submitter);
}

void async_logger::poll() {
  std::vector<log_ring_entry_t> backlogs;
  size_t rings_size = 0;
  {
    // no contention unless when new thread comes in
    std::lock_guard lk(mutex_);
    // to make this work requires a reserved log_rings_.
    rings_size = log_rings_.size();
  }
  // use do while to make sure nothing left
  do {
    bool empty = true;
    for (size_t i = 0; i < rings_size; ++i) {
      auto &ring = log_rings_[i];
      if (ring->batch_out(backlogs)) {
        empty = false;
      }
    }
    if (empty || signal_ || force_flush_) {
      signal_ = false;
      break;
    }
  } while (!stop_source_.stop_requested());
  assert(writing_buffer_.get());
  if (backlogs.empty() && writing_buffer_->empty()) {
    return;
  }
  for (auto &h : backlogs) {
    update_datetime(timestamp(h.second.ts_));
    write_prefix(h.second);
    auto out = std::back_inserter(*writing_buffer_);
    h.first(out);
    write_filename(h.second);
    fmt::format_to(out, ":{}\n", h.second.line_);
  }
  // flush to file
  if (force_flush_ || writing_buffer_->size() >= k_max_buffer - suggested_single_log_max_len) {
    output_file_.append(writing_buffer_->data(), static_cast<ptrdiff_t>(writing_buffer_->size()));
    writing_buffer_->clear();
    force_flush_ = false;
  }
}
async_logger::~async_logger() {
  stop();
  output_file_.force_flush();
  as_logger_single = nullptr;
}

}  // namespace coring::detail
