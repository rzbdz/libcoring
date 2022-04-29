#include <string>
#ifndef CORING_STR_UTILS
#define CORING_STR_UTILS
namespace coring::detail {
inline constexpr char str_util_parse_digits[] = {'9', '8', '7', '6', '5', '4', '3', '2', '1', '0', '1', '2', '3', '4',
                                                 '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F', '0', '1', '2',
                                                 '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

inline const char *str_util_parse_zero = str_util_parse_digits + 9;
inline void itoad(char buf[], int val) {
  int i = val;
  char *p = buf;
  do {
    int lsd = i % 10;
    i /= 10;
    *p++ = str_util_parse_zero[lsd];
  } while (i != 0);
  if (val < 0) *p++ = '-';
  *p = '\0';
  std::reverse(buf, p);
}
inline void itoa2(char buf[], int val, int rdx, int cap) {
  const char *zero_ = cap ? str_util_parse_zero : str_util_parse_digits + 25;
  auto i = static_cast<unsigned>(val);
  char *p = buf;
  do {
    unsigned lsd = i % rdx;
    i /= rdx;
    *p++ = zero_[lsd];
  } while (i != 0);
  *p = '\0';
  std::reverse(buf, p);
}
}  // namespace coring::detail
#endif