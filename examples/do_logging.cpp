// We won't use gtest for profiling reason
// #include "coring/debug.hpp"// Make output more clear...
#include "coring/async_logger.hpp"

using namespace coring;
#define LOG_FILE_NAME "test266"
void single_thread_async(int time, int divide) {
  async_logger as{LOG_FILE_NAME};
  int s = coring::detail::async_logger::ring_buffer_size * time / divide;
  std::cout << "AS init, begin as_logger_single thread testing (no full," << s << " )...) " << std::endl;
  as.run();
  //  as.run_busy();
  LOG_DEBUG("{}", 123);
  std::chrono::high_resolution_clock::time_point t0, t1;
  t0 = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < s; i++) {
    LOG_DEBUG(
        "Initialized InfUdDriver buffers: {} receive buffers ({} MB), {} transmit buffers ({} MB), took {:.1f} ms",
        50000, 97, 50, 0, 26.2);
  }
  t1 = std::chrono::high_resolution_clock::now();
  std::cout << ((t1 - t0).count() / (double)(s)) << "ns per submit" << std::endl;
  std::this_thread::sleep_for(std::chrono::seconds(3));
  as.stop();
}
double res[3];
void multi_thread_async(int time, int divide) {
  int scale = coring::detail::async_logger::ring_buffer_size * time / divide;
  async_logger as{LOG_FILE_NAME};
  std::cout << "AS init, begin multiple thread testing (no full, " << scale << ")..." << std::endl;
  as.run();
  auto f = [scale](int j) -> void {
    std::chrono::high_resolution_clock::time_point t0, t1;
    LOG_DEBUG("{}", 123);
    t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < scale; i++) {
      LOG_DEBUG(
          "Initialized InfUdDriver buffers: {} receive buffers ({} MB), {} transmit buffers ({} MB), took {:.1f} ms",
          50000, 97, 50, 0, 26.2);
    }
    t1 = std::chrono::high_resolution_clock::now();
    double span1 = std::chrono::duration_cast<std::chrono::duration<double>>(t1 - t0).count();
    // std::cout << std::this_thread::get_id() << " " << << "ns" << std::endl;
    res[j] = ((span1 / (double)(scale)) * 1e9);
  };
  std::jthread th1(f, 0);
  std::jthread th2(f, 1);
  std::jthread th3(f, 2);
  th1.join();
  th2.join();
  th3.join();
  as.stop();
  std::cout << res[0] << "ns per submit in thread 0" << std::endl;
  std::cout << res[1] << "ns per submit in thread 1" << std::endl;
  std::cout << res[2] << "ns per submit in thread 2" << std::endl;
}
int main(int argc, char *argv[]) {
  // sad that async_logger is designed in singleton pattern,
  // no way to run them all at a time.
  // single_thread_async(2, 1);
  if (argc < 4) {
    std::cout << "No, you have to input args:\n"
                 "- For multi-thread(3), use \"./program-name -m [multiple factor] [division factor]\"\n"
                 "multiple factor and division factor suggest a `(mf/df)*ring_size` msgs would be logged in a loop"
                 "- For single-thread, use \"./program-name -s\""
              << std::endl;
    return 0;
  }
  if (argv[1][0] != '-') {
    std::cout << "fuck you, don't trick me" << std::endl;
    abort();
  }
  int mf = std::stoi(argv[2]), df = std::stoi(argv[3]);
  if (argv[1][1] == 'm') {
    multi_thread_async(mf, df);
  } else if (argv[1][1] == 's') {
    single_thread_async(mf, df);
  } else {
  }
  return 0;
}