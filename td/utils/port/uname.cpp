#include "td/utils/port/uname.h"

#include "td/utils/port/config.h"

#include "td/utils/common.h"
#include "td/utils/logging.h"
#include "td/utils/misc.h"

#if TD_PORT_POSIX
#if TD_ANDROID
#include <sys/system_properties.h>
#else
#include <sys/utsname.h>
#endif
#endif

namespace td {

Slice get_operating_system_version() {
  static string result = []() -> string {
#if TD_PORT_POSIX
#if TD_ANDROID
    char version[PROP_VALUE_MAX + 1];
    int length = __system_property_get("ro.build.version.release", version);
    if (length > 0) {
      return "Android " + string(version, length);
    }
#else
    utsname name;
    int err = uname(&name);
    if (err == 0) {
      auto os_name = trim(PSTRING() << name.sysname << " " << name.release);
      if (!os_name.empty()) {
        return os_name;
      }
    }
#endif
    LOG(ERROR) << "Failed to identify OS name; use generic one";

#if TD_DARWIN_IOS
    return "iOS";
#elif TD_DARWIN_TV_OS
    return "tvOS";
#elif TD_DARWIN_WATCH_OS
    return "watchOS";
#elif TD_DARWIN_MAC
    return "macOS";
#elif TD_DARWIN
    return "Darwin";
#elif TD_ANDROID
    return "Android";
#elif TD_TIZEN
    return "Tizen";
#elif TD_LINUX
    return "Linux";
#elif TD_FREEBSD
    return "FreeBSD";
#elif TD_OPENBSD
    return "OpenBSD";
#elif TD_NETBSD
    return "NetBSD";
#elif TD_CYGWIN
    return "Cygwin";
#elif TD_EMSCRIPTEN
    return "Emscripten";
#else
    return "Unix";
#endif

#else

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP | WINAPI_PARTITION_SYSTEM)
    auto handle = GetModuleHandle(L"ntdll.dll");
    if (handle != nullptr) {
      using RtlGetVersionPtr = LONG(WINAPI *)(PRTL_OSVERSIONINFOEXW);
      RtlGetVersionPtr RtlGetVersion = reinterpret_cast<RtlGetVersionPtr>(GetProcAddress(handle, "RtlGetVersion"));
      if (RtlGetVersion != nullptr) {
        RTL_OSVERSIONINFOEXW os_version_info = {};
        os_version_info.dwOSVersionInfoSize = sizeof(os_version_info);
        if (RtlGetVersion(&os_version_info) == 0) {
          auto major = os_version_info.dwMajorVersion;
          auto minor = os_version_info.dwMinorVersion;
          bool is_server = os_version_info.wProductType != VER_NT_WORKSTATION;

          if (major == 10 && minor >= 0) {
            if (is_server) {
              return os_version_info.dwBuildNumber >= 17623 ? "Windows Server 2019" : "Windows Server 2016";
            }
            return "Windows 10";
          }
          if (major == 6 && minor == 3) {
            return is_server ? "Windows Server 2012 R2" : "Windows 8.1";
          }
          if (major == 6 && minor == 2) {
            return is_server ? "Windows Server 2012" : "Windows 8";
          }
          if (major == 6 && minor == 1) {
            return is_server ? "Windows Server 2008 R2" : "Windows 7";
          }
          if (major == 6 && minor == 0) {
            return is_server ? "Windows Server 2008" : "Windows Vista";
          }
          return is_server ? "Windows Server" : "Windows";
        }
      }
    }
#elif TD_WINRT
    return "Windows 10";
#endif

    LOG(ERROR) << "Failed to identify OS name; use generic one";
    return "Windows";
#endif
  }();
  return result;
}

}  // namespace td
