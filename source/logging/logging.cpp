
#include <coring/logging.hpp>
#include "coring/detail/thread.hpp"

namespace coring {
log_level LOG_LEVEL = LOG_LEVEL_CNT;

logger::submit_interface logger::submitter_;
// this must be completed in front end
const char *logger::log_level_map_[] = {"TRACE", "DEBUG", "INFO ", "WARN ", "ERROR", "FATAL"};

logger::logger(file_name_t file, int line, log_level level)
    : log_entry_{file, line, level, coring::detail::thread::tid_string()} {}

}  // namespace coring

namespace coring::detail {
log_entry::log_entry(file_name_t file, int line, log_level lv, const char *pid)
    : file_(file), line_(line), lv_(lv), ts_{log_timestamp::now()}, pid_string_{pid} {}
}  // namespace coring::detail
