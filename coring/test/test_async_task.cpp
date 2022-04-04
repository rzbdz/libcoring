// Just use g++-11 -std=c++20 -ggdb -O0 -o a.out test_async_task.cpp -I $CORING_DIRECTORY...
#include <iostream>
#include "coring/async/task.hpp"
#include "coring/async/async_task.hpp"
#include "coring/async/sync_wait.hpp"
using namespace coring;
std::coroutine_handle<> global_ptr;
struct TaskDelay {
  constexpr bool await_ready() noexcept { return false; }
  void await_suspend(std::coroutine_handle<> c) {
    // register the handle to the runtime library
    // register a timer to the o/s
    std::cout << "catch a co_await request, block the all co_await expr on the way" << std::endl;
    global_ptr = c;
  }
  void await_resume() {
    // I won't resume it...
  }
  TaskDelay(int) {}
};

// this is a simplified async_task<>
struct AsyncVoid {
  struct promise_type {
    AsyncVoid get_return_object() { return {}; }
    std::suspend_never initial_suspend() { return {}; }
    std::suspend_never final_suspend() noexcept { return {}; }
    void unhandled_exception() {}
    void return_void(){};
  };
};

async_task<int> do_something() {
  std::cout << "async_task start, not lazily" << std::endl;
  co_await TaskDelay(4);
  std::cout << "async_task prepare to exit, return 12345" << std::endl;
  co_return 12345;
}

task<int> do_something_lazy() {
  std::cout << "task start, lazily" << std::endl;
  co_await TaskDelay(4);
  std::cout << "task prepare to exit, return 12456" << std::endl;
  co_return 12456;
}

AsyncVoid container() {
  std::cout << "in container: " << std::endl;
  auto t = do_something();
  std::cout << "async_task constructed." << std::endl;
  std::cout << "co_await it: " << std::endl;
  int res = co_await t;
  std::cout << "in container, get return value: " << res << std::endl;
  std::cout << "container exit" << std::endl;
}

AsyncVoid container_delayed_await() {
  std::cout << "in container2: " << std::endl;
  auto t = do_something();
  std::cout << "async_task constructed." << std::endl;
  std::cout << "in container2, I am going to directly resume the async_task here" << std::endl;
  global_ptr.resume();
  std::cout << "in container2, then I co_await it" << std::endl;
  int res = co_await t;
  std::cout << "in container2, get return value: " << res << std::endl;
  std::cout << "container exit" << std::endl;
}

int main() {
  container();
  std::cout << "in main, I am going to resume the async_task" << std::endl;
  global_ptr.resume();
  container_delayed_await();
  std::cout << "in main, I am going to do something_lazy" << std::endl;
  []() -> AsyncVoid {
    auto t = do_something_lazy();
    std::cout << "now await task" << std::endl;
    auto res = co_await t;
    std::cout << "res 1" << res << std::endl;
    std::cout << "now await task again" << std::endl;
    auto res_again = co_await t;
    std::cout << "res 2" << res_again << std::endl;
  }();
  std::cout << "now resume the task" << std::endl;
  global_ptr.resume();
  std::cout << "main exit" << std::endl;
}