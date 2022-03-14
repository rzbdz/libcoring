/// RAW LOG
/// since async logger cannot boostrap itself.
#ifndef CORING_DEBUG_HPP
#define CORING_DEBUG_HPP
#include <stdio.h>
#ifndef ASSERT_EQ
#define ASSERT_EQ assert_eq
#endif
#define assert_eq(expr1, expr2)                                                                                       \
  if (!((expr1) == (expr2))) {                                                                                        \
    fprintf(stderr, "[%s %d] [%s]: Eq Assertion failed: " #expr1 ": %lu(0x%lx); " #expr2 ": %lu(0x%lx) \n", __FILE__, \
            __LINE__, __FUNCTION__, (size_t)(expr1), (size_t)(expr1), (size_t)(expr2), (size_t)(expr2));              \
    abort();                                                                                                          \
  } else

#ifndef EXPECT_EQ
#define EXPECT_EQ expect_eq
#endif
#define expect_eq(expr1, expr2)                                                                                    \
  if (!((expr1) == (expr2))) {                                                                                     \
    fprintf(stderr, "[%s %d] [%s]: Eq Expectation failed: " #expr1 ": %lu(0x%lx); " #expr2 ": %lu(0x%lx) \n",      \
            __FILE__, __LINE__, __FUNCTION__, (size_t)(expr1), (size_t)(expr1), (size_t)(expr2), (size_t)(expr2)); \
  } else
// don't use stdio and printf/scanf for there are malloc calls in them.
// use stderr who has no buffer
#ifndef LOG_DEBUG_RAW

#ifndef RAW_NDEBUG
#define LOG_DEBUG_RAW(fmt, args...) \
  fprintf(stderr, "[%s %d\t::%s()] " fmt "\n", __FILE__, __LINE__, __FUNCTION__, ##args)

#else
#define LOG_DEBUG_RAW(fmt, args...) static_cast<void>(0)
#endif

#endif

#ifndef LOG_INFO_RAW
#define LOG_INFO_RAW(fmt, args...) \
  fprintf(stderr, "[%s %d\t::%s()] " fmt "\n", __FILE__, __LINE__, __FUNCTION__, ##args)
#endif

#endif  // CORING_DEBUG_HPP
