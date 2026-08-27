// Minimal assertion harness so the pure layers can be built and run with
// nothing but g++ — no PlatformIO, no toolchain download, no hardware.
#pragma once

#include <cstdio>
#include <cstdlib>
#include <cmath>

namespace harness {
inline int g_checks = 0;
inline int g_failures = 0;
inline const char* g_current = "";
}  // namespace harness

#define TEST(name)                     \
  harness::g_current = name;           \
  std::printf("  %s\n", name);

#define CHECK(cond)                                                        \
  do {                                                                     \
    ++harness::g_checks;                                                   \
    if (!(cond)) {                                                         \
      ++harness::g_failures;                                               \
      std::printf("    FAIL %s:%d in [%s]: %s\n", __FILE__, __LINE__,      \
                  harness::g_current, #cond);                              \
    }                                                                      \
  } while (0)

#define CHECK_EQ(a, b)                                                     \
  do {                                                                     \
    ++harness::g_checks;                                                   \
    auto _a = (a);                                                         \
    auto _b = (b);                                                         \
    if (!(_a == _b)) {                                                     \
      ++harness::g_failures;                                               \
      std::printf("    FAIL %s:%d in [%s]: %s == %s (got %lld vs %lld)\n", \
                  __FILE__, __LINE__, harness::g_current, #a, #b,          \
                  (long long)_a, (long long)_b);                           \
    }                                                                      \
  } while (0)

#define CHECK_NEAR(a, b, tol)                                              \
  do {                                                                     \
    ++harness::g_checks;                                                   \
    double _a = (double)(a);                                               \
    double _b = (double)(b);                                               \
    if (std::fabs(_a - _b) > (tol)) {                                      \
      ++harness::g_failures;                                               \
      std::printf("    FAIL %s:%d in [%s]: %s ~= %s (got %f vs %f)\n",     \
                  __FILE__, __LINE__, harness::g_current, #a, #b, _a, _b); \
    }                                                                      \
  } while (0)

inline int harness_report() {
  std::printf("\n%d checks, %d failures\n", harness::g_checks, harness::g_failures);
  return harness::g_failures == 0 ? 0 : 1;
}
