#if 0

namespace td {

#if TD_PORT_WINDOWS

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP | WINAPI_PARTITION_SYSTEM)
Fd &Fd::Stderr() {
  static auto handle = GetStdHandle(STD_ERROR_HANDLE);
  LOG_IF(FATAL, handle == INVALID_HANDLE_VALUE) << "Failed to get stderr";
  static auto fd = Fd(Fd::Type::FileFd, Fd::Mode::Reference, handle);
  return fd;
}
Fd &Fd::Stdin() {
  static auto handle = GetStdHandle(STD_INPUT_HANDLE);
  LOG_IF(FATAL, handle == INVALID_HANDLE_VALUE) << "Failed to get stdin";
  static auto fd = Fd(Fd::Type::FileFd, Fd::Mode::Reference, handle);
  return fd;
}
Fd &Fd::Stdout() {
  static auto handle = GetStdHandle(STD_OUTPUT_HANDLE);
  LOG_IF(FATAL, handle == INVALID_HANDLE_VALUE) << "Failed to get stdout";
  static auto fd = Fd(Fd::Type::FileFd, Fd::Mode::Reference, handle);
  return fd;
}
#else
Fd &Fd::Stderr() {
  static Fd result;
  result = Fd();
  return result;
}
Fd &Fd::Stdin() {
  static Fd result;
  result = Fd();
  return result;
}
Fd &Fd::Stdout() {
  static Fd result;
  result = Fd();
  return result;
}
#endif

#endif

}  // namespace td
#endif
