#include "async_logger.hpp"
#include "../debug.hpp"
using namespace coring;
void single_thread_async() {
  async_logger as{};
  LOG_DEBUG_RAW("as init");
  as.run();
  //  as.run_busy();
  LOG_DEBUG("{}", 123);
  int s = coring::detail::async_logger::ring_buffer_size * 2;
  std::chrono::high_resolution_clock::time_point t0, t1;
  t0 = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < s; i++) {
    LOG_DEBUG(
        "Initialized InfUdDriver buffers: {} receive buffers ({} MB), {} transmit buffers ({} MB), took {:.1f} ms",
        50000, 97, 50, 0, 26.2);
  }
  t1 = std::chrono::high_resolution_clock::now();
  std::cout << ((t1 - t0).count() / (double)(s)) << "ns" << std::endl;
  std::this_thread::sleep_for(std::chrono::seconds(3));
  as.stop();
}

double res[3];
constexpr int scale = 2048;
void multi_thread_async() {
  async_logger as{"scale"};
  LOG_DEBUG_RAW("as init");
  as.run();
  auto f = [](int j) -> void {
    std::chrono::high_resolution_clock::time_point t0, t1;
    LOG_DEBUG("{}", 123);
    t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < scale; i++) {
      LOG_DEBUG("agasfgsagasf {}{:.3f}{}", 1234567, 1.6454, i);
      // LOG_DEBUG("agasfgsagasf {}{}", 1234567 , i);
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
  std::cout << res[0] << "ns" << std::endl;
  std::cout << res[1] << "ns" << std::endl;
  std::cout << res[2] << "ns" << std::endl;
}
void single_thread_sync() {
  LOG_DEBUG_RAW("sync single as init");
  //  as.run_busy();
  LOG_DEBUG_SYNC("{}", 123);
  int s = coring::detail::async_logger::ring_buffer_size * 2;
  std::chrono::high_resolution_clock::time_point t0, t1;
  t0 = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < s; i++) {
    LOG_DEBUG_SYNC(
        "Initialized InfUdDriver buffers: {} receive buffers ({} MB), {} transmit buffers ({} MB), took {:.1f} ms",
        50000, 97, 50, 0, 26.2);
  }
  t1 = std::chrono::high_resolution_clock::now();
  std::cout << ((t1 - t0).count() / (double)(s)) << "ns" << std::endl;
  std::this_thread::sleep_for(std::chrono::seconds(3));
}
void multi_thread_sync() {
  coring::detail::sync_bf.clear();
  LOG_DEBUG_RAW("sync as init");
  auto f = [](int j) -> void {
    std::chrono::high_resolution_clock::time_point t0, t1;
    LOG_DEBUG_SYNC("{}", 123);
    t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < scale; i++) {
      LOG_DEBUG_SYNC("SYNC agasfgsagasf {}{:.3f}{}", 1234567, 1.6454, i);
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
  std::cout << res[0] << "ns" << std::endl;
  std::cout << res[1] << "ns" << std::endl;
  std::cout << res[2] << "ns" << std::endl;
}
int main() {
  single_thread_async();
  multi_thread_async();
  single_thread_sync();
  multi_thread_sync();
  return 0;
}