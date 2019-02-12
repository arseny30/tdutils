#pragma once
#define CHECK(x)                                    \
  if (!(x)) {                                       \
    ::td::detail::do_check(#x, __FILE__, __LINE__); \
  }
#define DCHECK(x) CHECK(x)
namespace td {
namespace detail {
void do_check(const char *message, const char *file, int line);
}
}  // namespace td
