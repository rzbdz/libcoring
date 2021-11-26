
#ifndef CORING_DEBUG_HPP
#define CORING_DEBUG_HPP

#define assert_eq(expr1, expr2)                                                                                                                                                                  \
  if (!((expr1) == (expr2))) {                                                                                                                                                                   \
    fprintf(stderr, "[%s %d] [%s]: Eq Assertion failed: " #expr1 ": %lu(0x%lx); " #expr2 ": %lu(0x%lx) \n", __FILE__, __LINE__, __FUNCTION__, (size_t)(expr1), (size_t)(expr1), (size_t)(expr2), \
            (size_t)(expr2));                                                                                                                                                                    \
    abort();                                                                                                                                                                                     \
  } else
#define expect_eq(expr1, expr2)                                                                                                                                                                    \
  if (!((expr1) == (expr2))) {                                                                                                                                                                     \
    fprintf(stderr, "[%s %d] [%s]: Eq Expectation failed: " #expr1 ": %lu(0x%lx); " #expr2 ": %lu(0x%lx) \n", __FILE__, __LINE__, __FUNCTION__, (size_t)(expr1), (size_t)(expr1), (size_t)(expr2), \
            (size_t)(expr2));                                                                                                                                                                      \
  } else
// don't use stdio and printf/scanf for there are malloc calls in them.
// use stderr who has no buffer
#ifndef NDEBUG
#define LOG_DEBUG(fmt, args...) fprintf(stderr, "[%s %d\t::%s()] " fmt "\n", __FILE__, __LINE__, __FUNCTION__, ##args)
#define P_P(x) LOG_DEBUG(#x " %lx", (size_t)(x));
#define P_D(x) LOG_DEBUG(#x " %lu", (size_t)(x));
#else
#define LOG_DEBUG(fmt, args...) static_cast<void>(0)
#endif
#define LOG_INFO(fmt, args...) fprintf(stderr, "[%s %d\t::%s()] " fmt "\n", __FILE__, __LINE__, __FUNCTION__, ##args)

#endif  // CORING_DEBUG_HPP
