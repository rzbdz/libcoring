#include "io_utils.hpp"

void coring::detail::panic(std::string_view sv, int err) {
#ifndef NDEBUG
  // https://stackoverflow.com/questions/77005/how-to-automatically-generate-a-stacktrace-when-my-program-crashes
  void *array[32];
  size_t size;

  // get void*'s for all entries on the stack
  size = backtrace(array, 32);

  // print out all the frames to stderr
  fprintf(stderr, "Error: errno %d:\n", err);
  backtrace_symbols_fd(array, size, STDERR_FILENO);

  // __asm__("int $3");
#endif

  throw std::system_error(err, std::generic_category(), sv.data());
}
