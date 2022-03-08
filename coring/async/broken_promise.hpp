///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CORING_ASYNC_BROKEN_PROMISE
#define CORING_ASYNC_BROKEN_PROMISE

#include <stdexcept>

namespace coring {
/// \brief
/// Exception thrown when you attempt to retrieve the result of
/// a task that has been detached from its promise/coroutine.
class broken_promise : public std::logic_error {
 public:
  broken_promise() : std::logic_error("broken promise") {}
};
}  // namespace coring

#endif
