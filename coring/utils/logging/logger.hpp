#ifndef CORING_ETC_LOGGING_LOGGER_H
#define CORING_ETC_LOGGING_LOGGER_H

// logger is the backend of async logging module

#include <mutex>
#include <condition_variable>
#include <latch>

#include "../noncopyable.hpp"

namespace coring {
class logger : noncopyable {
 private:
  std::string file_name_;
  std::mutex mutex_;
  std::condition_variable cond_;
  std::latch latch_;
};
}  // namespace coring
#endif  // CORING_ETC_LOGGING_LOGGER_H