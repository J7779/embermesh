// Wall-clock bounds in tests are written for an optimised build on an ordinary machine. Sanitizer
// builds on shared CI runners are many times slower: multiply a bound by timing_slack() so that it
// still catches the pathological case (orders of magnitude) without failing on a slow host.
#pragma once

inline double timing_slack() {
#if defined(__SANITIZE_ADDRESS__)
  return 8.0;
#elif defined(__has_feature)
#if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer) || __has_feature(memory_sanitizer)
  return 8.0;
#else
  return 1.0;
#endif
#else
  return 1.0;
#endif
}
