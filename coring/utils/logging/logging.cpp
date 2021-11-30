
#include "logging.hpp"

namespace coring {
thread_local timestamp last_time_;
thread_local char time_buffer_[40];
}  // namespace coring

namespace coring {
const char *logger::log_level_map_[] = {"TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"};

logger::logger(logger::file_name_t file, int line, logger::log_level level) : log_entry_{file, line, timestamp{}, level} {}

logger::log_entry::log_entry(logger::file_name_t file, int line, timestamp ts, logger::log_level lv) : file_(file), line_(line), ts_(std::move(ts)), lv_(lv) {}

}  // namespace coring
