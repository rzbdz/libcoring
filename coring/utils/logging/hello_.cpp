#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <functional>
#include "../fmt/format.h"
#include "timestamp.hpp"
#include "logging.hpp"

#define CAST_CSTRING
template <typename T>
struct should_trans : public std::false_type {};

#ifdef CAST_CSTRING
template <>
struct should_trans<const char *> : public std::true_type {};

template <>
struct should_trans<char *> : public std::true_type {};

template <>
struct should_trans<const char *&> : public std::true_type {};

template <>
struct should_trans<char *&> : public std::true_type {};
#endif

template <typename T>
constexpr bool should_trans_v = should_trans<T>::value;

template <typename T>
struct trans_type {
  typedef T use_type;
};

template <>
struct trans_type<char *> {
  typedef std::string use_type;
};

template <>
struct trans_type<const char *> {
  typedef std::string use_type;
};

template <typename T>
auto trans(T v) {
  // make sure pass by value instead of reference
  // it's ok when char* is just a pointer
  using use_type = typename trans_type<T>::use_type;
  static_assert(!std::is_same_v<use_type, T>);
  static_assert(std::is_constructible_v<use_type, T>);
  return use_type(v);
}

template <typename T>
struct should_copy : public std::true_type {};

template <int N>
struct should_copy<const char (&)[N]> : public std::false_type {};

template <typename T>
constexpr bool should_copy_v = should_copy<T>::value;

// template <typename Arg>
// auto log_tuple_helper(Arg &&a) {
//   if constexpr (should_copy_v<Arg>) {
//     std::cout << "do call last one";
//     if constexpr (should_trans_v<Arg>) {
//       return std::make_tuple(trans(a));
//     } else {
//       return std::make_tuple(a);
//     }
//   } else {
//     return std::forward_as_tuple(a);
//   }
// }
template <typename... Args>
auto log_tuple_helper(Args &&...) requires(sizeof...(Args) == 0) {
  return std::tie();
}
template <typename Arg, typename... Args>
auto log_tuple_helper(Arg &&first_arg, Args &&...args) {
  if constexpr (should_copy_v<Arg>) {
    if constexpr (should_trans_v<Arg>) {
      return std::tuple_cat(std::make_tuple(trans(first_arg)), log_tuple_helper(args...));
    } else {
      return std::tuple_cat(std::make_tuple(first_arg), log_tuple_helper(args...));
    }
  } else {
    return std::tuple_cat(std::forward_as_tuple(first_arg), log_tuple_helper(args...));
  }
}

// template <typename OutputIt, typename... Args>
// auto make_log_tuple(OutputIt &&out, fmt::format_string<Args...> fmt, Args &&...args) {
//   return std::tuple_cat(std::make_tuple(std::ref(out), fmt::string_view(fmt)), make_args_tuple(args...));
// }
//
// auto wrapper = [](auto &&o, fmt::string_view fmt, auto &&...args) { fmt::vformat_to(o, fmt, fmt::make_format_args(args...)); };
//
// template <typename OutputIt, typename... Args>
// auto make_log_task(OutputIt &&out, fmt::format_string<Args...> fmt, Args &&...args) {
//   return [t = make_log_tuple(out, std::move(fmt), args...)]() -> void { std::apply(wrapper, t); };
// }
template <typename... Args>
auto make_log_tuple(fmt::string_view fmt, Args &&...args) {
  return std::tuple_cat(std::make_tuple(fmt), log_tuple_helper(args...));
}

auto wrapper = [](auto &&o, fmt::string_view fmt, auto &&...args) { fmt::vformat_to(o, fmt, fmt::make_format_args(args...)); };

template <typename... Args>
auto make_log_task(fmt::format_string<Args...> fmt, Args &&...args) {
  return [t = make_log_tuple(std::move(fmt), args...)](auto &&output_it) -> void { std::apply(wrapper, std::tuple_cat(std::tie(output_it), t)); };
}

void test_different_string() {
  auto out = std::vector<char>();
  auto o = std::back_inserter(out);

  char *array_char = new char[50];
  static_assert(should_trans_v<decltype(array_char)>);

  char _mych_[] = "This is a string in array originally";
  for (int i = 0; i < 37; i++) array_char[i] = _mych_[i];
  array_char[5] = 'T';

  auto st = std::make_tuple(std::string(array_char));

  char *as_ptr = _mych_;
  std::string string_char = "This is a std::string";

  auto kk = make_log_tuple("{}, {}, {}\n", array_char, as_ptr, string_char);

  auto log_task = make_log_task("{}, {}, {}\n", array_char, as_ptr, string_char);
  auto log_t2 = make_log_task("{}, {}, {}, {}", 1.553, "logt2", "???", 34344);
  log_t2(o);
  //  fmt::format_to(o, "{}", 1.535);
  int i = 1;

  log_task(o);
  delete[] array_char;
  char *_to_override_ = new char[50];
  memset(_to_override_, '0', 50);

  log_task(o);

  for (auto i : out) {
    std::cout << i;
  }
  std::cout << std::get<0>(st) << "\n";
}

// int try_tuple(void) {
//   auto out = std::vector<char>();
//   auto o = std::back_inserter(out);
//   auto my_funt = [](auto &&o, fmt::string_view fmt, auto &&...args) { fmt::vformat_to(o, fmt, fmt::make_format_args(args...)); };
//   auto function1 = [&my_funt, tt = make_log_tuple(o, "?I am curious{},{:d},{:.1}", "abcde", 12345, 1.54564)]() -> void { std::apply(my_funt, tt); };
//   auto function2 = [&my_funt, tt = make_log_tuple(o, "???I am curious what happend? {1:x},{0},{2:.3}", "abcde", 12345, 1.54564)]() -> void { std::apply(my_funt, tt); };
//   function2();
//   // function1();
//   for (auto i : out) {
//     std::cout << i;
//   }
//   std::cout << std::endl;
//   return 0;
//}

int try_closure(void) {
  auto out = std::vector<char>();
  out.reserve(4096);
  std::string test_life_problem = std::string("life string");
  //  auto args = fmt::make_format_args(42, "abc", 5.1234, test_life_problem);
  //  fmt::vformat_to(std::back_inserter(out), fmt::to_string_view("{}, {}, {:.2}, {}\n"), args);
  //  fmt::vformat_to(std::back_inserter(out), fmt::to_string_view("{}, {}, {:.2}, {}\n"), args);
  //  fmt::vformat_to(std::back_inserter(out), fmt::to_string_view("{}, {}, {:.2}, {}\n"), args);
  auto my_funt = [](auto &&o, std::string_view fmt, auto &&...args) { fmt::vformat_to(o, fmt, fmt::make_format_args(args...)); };
  auto t12t = std::make_tuple(std::back_inserter(out), "{}, {}, {:.2}, {}\n", 42, "abc", 5.1234, test_life_problem);
  decltype(t12t) b;
  auto function1 = [&my_funt, tt = std::make_tuple(std::back_inserter(out), "{}, {}, {:.2}, {}\n", 42, "abc", 5.1234, test_life_problem)]() -> void { std::apply(my_funt, tt); };
  test_life_problem = "new string";
  auto function2 = [&my_funt, tt1 = std::make_tuple(std::back_inserter(out), "{}, {}, {:.2}, {}\n", 42, "abc", 5.1234, std::ref(test_life_problem))]() -> void { std::apply(my_funt, tt1); };

  std::string *nn = new std::string("newa string");
  { nn->size(); }
  //  fmt::vformat_to(std::back_inserter(out), fmt::to_string_view("{}, {}, {:.2}, {}\n"), args);
  //  fmt::vformat_to(std::back_inserter(out), fmt::to_string_view("{}, {}, {:.2}, {}\n"), args);
  //  fmt::vformat_to(std::back_inserter(out), fmt::to_string_view("{}, {}, {:.2}, {}\n"), args);
  //  std::cout << *nn << "\n";
  // https://stackoverflow.com/questions/68675303/how-to-create-a-function-that-forwards-its-arguments-to-fmtformat-keeping-the
  // std::vector<std::tuple> tuple_list(20); wrong
  //

  std::cout << &function1 << " " << &function2 << std::endl;
  std::jthread th1(function1);
  th1.join();
  std::jthread th2(function2);
  th2.join();
  // https://stackoverflow.com/questions/24681182/does-boostmake-tuple-make-copies
  for (auto &c : out) {
    fmt::print("{}", c);
  }
  fmt::print("\n");
  return 0;
}
void test_chrono() {
  std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds> now;
  now = std::chrono::system_clock::now();
  // this is a duration
  std::chrono::duration<int64_t, std::nano> ss = now.time_since_epoch();
  // so is this one
  auto hh = std::chrono::floor<std::chrono::days>(ss);
  // this is a time point
  std::chrono::time_point<std::chrono::system_clock, std::chrono::days> tp = std::chrono::floor<std::chrono::days>(now);
  // hhmmss use a duration
  std::chrono::hh_mm_ss<std::chrono::milliseconds> tod{std::chrono::duration_cast<std::chrono::milliseconds>(ss - hh)};
  // ymd use a time point
  const std::chrono::year_month_day ymd(tp);
  std::cout << static_cast<int>(ymd.year()) << static_cast<unsigned>(ymd.month()) << static_cast<unsigned>(ymd.day()) << tod.hours().count() << tod.minutes().count() << tod.seconds().count()
            << tod.subseconds().count();
}

void test_timestamp() {
  coring::timestamp ts1;
  std::cout << ts1.to_formatted_string() << std::endl;
  coring::timestamp copy = ts1;
  std::cout << copy.to_formatted_string() << std::endl;
  coring::timestamp ts2;
  std::cout << ts2.to_formatted_string() << std::endl;
  std::cout << ts1.same_second(ts2) << std::endl;
}
struct TEST {
  static void test_logger() {
    auto lg = coring::logger(__FILE__, __LINE__, coring::logger::INFO);
    auto out = std::vector<char>();
    auto o = std::back_inserter(out);
    auto f = lg.try_log("{2:.3f}, {1}, {0:04}", 1, "aaa", 5.51321);
    f(o);
    for (auto i : out) std::cout << i;
    std::cout << static_cast<std::string>(lg.log_entry_.ts_) << std::endl;
    std::cout << lg.log_entry_.file_.data_ << std::endl;
    std::cout << coring::logger::log_level_map_[lg.log_entry_.lv_] << std::endl;
  }
};

int main(void) {
  //  test_different_string();
  //   test_chrono();
      test_timestamp();
  // TEST::test_logger();

  return 0;
}