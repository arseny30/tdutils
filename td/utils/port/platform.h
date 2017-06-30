#pragma once

// clang-format off

/*** Platform macros ***/
#if defined(_WIN32)
  #if defined(__cplusplus_winrt)
    #define TD_WINRT 1
  #endif
  #if defined(__cplusplus_cli)
    #define TD_CLI 1
  #endif
  #define TD_WINDOWS 1
#elif defined(__APPLE__)
  #include "TargetConditionals.h"
  #if TARGET_IPHONE_SIMULATOR
    // iOS/watchOS Simulator
  #elif TARGET_OS_IPHONE
    // iOS/watchOS device
  #elif TARGET_OS_MAC
    // Other kinds of Mac OS
  #else
    #warning "Probably unsupported Apple platform. Feel free to try to compile"
  #endif
  #define TD_DARWIN 1
#elif defined(ANDROID) || defined(__ANDROID__)
  #define TD_ANDROID 1
#elif defined(TIZEN_DEPRECATION)
  #define TD_TIZEN 1
#elif defined(__linux__)
  #define TD_LINUX 1
#elif defined(__CYGWIN__)
  #define TD_CYGWIN 1
#elif defined(__unix__)  // all unices not caught above
  #warning "Probably unsupported Unix platform. Feel free to try to recompile"
  #define TD_CYGWIN 1
#else
  #error "Probably unsupported platform. Feel free to remove the error and try to recompile"
#endif

#if defined(__ICC) || defined(__INTEL_COMPILER)
  #define TD_INTEL 1
#elif defined(__clang__)
  #define TD_CLANG 1
#elif defined(__GNUC__) || defined(__GNUG__)
  #define TD_GCC 1
#elif defined(_MSC_VER)
  #define TD_MSVC 1
#else
  #warning "Probably unsupported compiler. Feel free to try to compile"
#endif

#if TD_GCC || TD_CLANG || TD_INTEL
  #define WARN_UNUSED_RESULT __attribute__((warn_unused_result))
  #define ATTRIBUTE_FORMAT_PRINTF(from, to) __attribute__((format(printf, from, to)))
#else
  #define WARN_UNUSED_RESULT
  #define ATTRIBUTE_FORMAT_PRINTF(from, to)
#endif

#if TD_CLANG || TD_GCC || TD_INTEL
  #define likely(x) __builtin_expect(x, 1)
  #define unlikely(x) __builtin_expect(x, 0)
#else
  #define likely(x) x
  #define unlikely(x) x
#endif

#if TD_MSVC
  #define TD_UNUSED __pragma(warning(suppress : 4100))
#elif TD_CLANG || TD_GCC || TD_INTEL
  #define TD_UNUSED __attribute__((unused))
#else
  #define TD_UNUSED
#endif

#define TD_HAS_ATOMIC_SHARED_PTR 1

// No atomic operations on shared_ptr in libstdc++ before 5.0
// see https://gcc.gnu.org/bugzilla/show_bug.cgi?id=57250
#if __GLIBCXX__
#undef TD_HAS_ATOMIC_SHARED_PTR
#endif

// Also no atomic operations on shared_ptr when clang __has_feature(cxx_atomic) is defined and zero
#if defined(__has_feature)
#if !__has_feature(cxx_atomic)
#undef TD_HAS_ATOMIC_SHARED_PTR
#endif
#endif

// clang-format on
