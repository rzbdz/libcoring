
#ifndef CORING_IO_CONTEXT_HPP
#define CORING_IO_CONTEXT_HPP

#include "coring/async/task.hpp"
#include <vector>
namespace coring {
class io_context {
 public:
  void run() {}
  template <typename T, bool nothrow>
  T run(const coring::task<T> &t) noexcept(nothrow) {
    return {};
  }

 private:
  // TODO(pan): as far as I know, std::function
  // can be used to implement true generic container for
  // template class.
  // When make everything inherit from a IClass is painful.
  // But maybe task could be modified to adapt that.
  // std::vector<coring::task> mission_queue;
  // or something like that:
  // std::vector<std::shared_ptr<coring::task_interface>> mission_queue;
};
}  // namespace coring
#endif  // CORING_IO_CONTEXT_HPP
