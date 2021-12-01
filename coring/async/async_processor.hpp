
#ifndef CORING_ASYNC_PROCESSOR_HPP
#define CORING_ASYNC_PROCESSOR_HPP

#include <coroutine>
#include "coring/io/io_context.hpp"
namespace coring {

template <typename TaskType>
class async_processor {
 public:
  async_processor(coring::io_context ctx, TaskType &&task) {}
  struct exec_awaitable {
    void await_suspend(std::coroutine_handle<> h) {
      // move it to another thread;
      // and awaits it
    }
    using value_type = typename TaskType::value_type;
    value_type await_resume() { return {}; }
  };
  exec_awaitable exec() { return {}; }

 private:
  std::coroutine_handle<> callback_;
};
}  // namespace coring
#endif  // CORING_ASYNC_PROCESSOR_HPP
