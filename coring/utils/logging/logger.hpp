#ifndef CORING_ETC_LOGGING_LOGGER_H
#define CORING_ETC_LOGGING_LOGGER_H

#include <mutex>
#include <condition_variable>

#include "../noncopyable.hpp"

namespace coring {
class logger : noncopyable {
 private:
  std::mutex mutex_;
  std::condition_variable cond_;
};
}  // namespace coring
#endif  // CORING_ETC_LOGGING_LOGGER_H