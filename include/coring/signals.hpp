/// signals.hpp
/// Created by panjunzhong@outlook.com on 2022/4/25.

#ifndef CORING_SIGNALS_HPP
#define CORING_SIGNALS_HPP

#include <csignal>
namespace coring {
class signal_set {
 public:
  static signal_set sigint_for_context() {
    signal_set s{};
    s.add(SIGINT);
    s.block_on_proc();
    return s;
  }
  signal_set() { sigemptyset(&set_); }
  template <typename... Args>
  requires(std::is_constructible_v<decltype(SIGINT), Args>, ...) void add(Args... args) {
    (sigaddset(&set_, args), ...);
  }
  void remove(decltype(SIGINT) signo) { sigdelset(&set_, signo); }
  void del(decltype(SIGINT) signo) { sigdelset(&set_, signo); }
  void block_on_proc() { sigprocmask(SIG_BLOCK, &set_, nullptr); }
  void block_on_thread() { pthread_sigmask(SIG_BLOCK, &set_, nullptr); }
  sigset_t *get_set() { return &set_; }

 private:
  sigset_t set_{};
};
}  // namespace coring

#endif  // CORING_SIGNALS_HPP
