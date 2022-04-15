
#ifndef CORING_EXECUTE_HPP
#define CORING_EXECUTE_HPP

#include "thread.hpp"
#include "coring/async/is_awaitable.hpp"
namespace coring {

template <class Exec, class Awaitable>
concept Executor = std::is_move_constructible_v<std::remove_cvref<Awaitable>> && is_awaitable_v<Awaitable> &&
    requires(Exec &e, Awaitable &&task) {
  e.execute(std::forward<Awaitable>(task));
};

template <typename Task, Executor<Task> Exec>
void execute(Exec *exec, Task &&task) {
  exec->execute(std::forward<Task>(task));
}

template <typename Task, Executor<Task> Exec>
void execute(Exec &exec, Task &&task) {
  exec.execute(std::forward<Task>(task));
}

template <class Sched, class Awaitable>
concept Scheduler = std::is_move_constructible_v<std::remove_cvref<Awaitable>> && is_awaitable_v<Awaitable> &&
    requires(Sched &sched, Awaitable &&task) {
  sched.schedule(std::forward<Awaitable>(task));
};

template <typename Task, Scheduler<Task> Sched>
void schedule(Sched *sched, Task &&task) {
  sched->schedule(std::forward<Task>(task));
}

template <typename Task, Scheduler<Task> Sched>
void schedule(Sched &sched, Task &&task) {
  sched.schedule(std::forward<Task>(task));
}

template <typename Task, Scheduler<Task> Sched>
void submit(Sched *sched) {
  sched->submit();
}

template <typename Task, Scheduler<Task> Sched>
void submit(Sched &sched) {
  sched.submit();
}
}  // namespace coring
#endif  // CORING_EXECUTE_HPP
