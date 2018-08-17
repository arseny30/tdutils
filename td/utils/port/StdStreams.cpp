#include "td/utils/port/StdStreams.h"

namespace td {

namespace {
template <class T>
FileFd create(T handle) {
  return FileFd::from_native_fd(NativeFd(handle, true)).move_as_ok();
}
}  // namespace
FileFd &Stdin() {
  static FileFd res = create(
#if TD_PORT_POSIX
      0
#elif TD_PORT_WINDOWS
      GetStdHandle(STD_INPUT_HANDLE)
#endif
  );
  return res;
}
FileFd &Stdout() {
  static FileFd res = create(
#if TD_PORT_POSIX
      1
#elif TD_PORT_WINDOWS
      GetStdHandle(STD_OUTPUT_HANDLE)
#endif
  );
  return res;
}
FileFd &Stderr() {
  static FileFd res = create(
#if TD_PORT_POSIX
      2
#elif TD_PORT_WINDOWS
      GetStdHandle(STD_ERROR_HANDLE)
#endif
  );
  return res;
}
}  // namespace td
