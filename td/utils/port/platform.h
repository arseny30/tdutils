#pragma once

// clang-format off

/*** Platform macros ***/
#if defined(_WIN32)
  // define something for Windows (32-bit and 64-bit, this part is common)
  #if defined(_WIN64)
    // define something for Windows (64-bit only)
  #endif
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
    #define TD_MAC 1
  #elif TARGET_OS_IPHONE
    // iOS/watchOS device
    #define TD_MAC 1
  #elif TARGET_OS_MAC
    // Other kinds of Mac OS
    #define TD_MAC 1
  #else
    #error "Probably unsupported Apple platform. Feel free to remove the error and try to recompile"
    #define TD_MAC 1
  #endif
#elif defined(ANDROID) || defined(__ANDROID__)
  #define TD_ANDROID 1
#elif defined(TIZEN_DEPRECATION)
  #define TD_TIZEN 1
#elif defined(__linux__)
  #define TD_LINUX 1
#elif defined(__unix__)  // all unices not caught above
  #error "Probably unsupported Unix platform. Feel free to remove the error and try to recompile"
  #define TD_LINUX 1
#else
  #error "Probably unsupported platform. Feel free to remove the error and try to recompile"
#endif

#if defined(__clang__)
  #define TD_CLANG 1
#elif defined(__ICC) || defined(__INTEL_COMPILER)
  #define TD_INTEL 1
#elif defined(__GNUC__) || defined(__GNUG__)
  #define TD_GCC 1
#elif defined(_MSC_VER)
  #define TD_MSVC 1
#else
  #error "Probably unsupported compiler. Feel free to remove the error and try to recompile"
#endif

#if TD_GCC || TD_CLANG
  #define WARN_UNUSED_RESULT __attribute__((warn_unused_result))
  #define ATTRIBUTE_FORMAT_PRINTF(from, to) __attribute__((format(printf, from, to)))
#else
  #define WARN_UNUSED_RESULT
  #define ATTRIBUTE_FORMAT_PRINTF(from, to)
#endif

#if TD_CLANG || TD_GCC
  #define likely(x) __builtin_expect(x, 1)
  #define unlikely(x) __builtin_expect(x, 0)
#else
  #define likely(x) x
  #define unlikely(x) x
#endif

#if TD_MSVC
  #define TD_UNUSED __pragma(warning(suppress : 4100))
#elif TD_CLANG || TD_GCC
  #define TD_UNUSED __attribute__((unused))
#else
  #define TD_UNUSED
#endif

// clang-format on
