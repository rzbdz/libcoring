
#ifndef CORING_EXECUTE_HPP
#define CORING_EXECUTE_HPP

#include "thread.hpp"
namespace coring {
template <typename Exec, typename Task>
void execute(Exec *exec, Task &&task) {
  exec->execute(std::forward<Task>(task));
}
template <typename Exec, typename Task>
void schedule(Exec *exec, Task &&task) {
  exec->schedule(std::forward<Task>(task));
}
template <typename Exec>
void submit(Exec *exec) {
  exec->submit();
}
template <typename Exec, typename Task>
void execute(Exec &exec, Task &&task) {
  exec.execute(std::forward<Task>(task));
}
template <typename Exec, typename Task>
void schedule(Exec &exec, Task &&task) {
  exec.schedule(std::forward<Task>(task));
}
template <typename Exec>
void submit(Exec &exec) {
  exec.submit();
}
}  // namespace coring
#endif  // CORING_EXECUTE_HPP
