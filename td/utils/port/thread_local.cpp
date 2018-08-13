#include "td/utils/port/thread_local.h"

#include "td/utils/logging.h"

namespace td {

namespace detail {

static TD_THREAD_LOCAL int32 thread_id_;
static TD_THREAD_LOCAL std::vector<std::unique_ptr<Destructor>> *thread_local_destructors;

void add_thread_local_destructor(std::unique_ptr<Destructor> destructor) {
  if (thread_local_destructors == nullptr) {
    thread_local_destructors = new std::vector<std::unique_ptr<Destructor>>();
  }
  thread_local_destructors->push_back(std::move(destructor));
}

}  // namespace detail

void clear_thread_locals() {
  // ensure that no destructors were added during destructors invokation
  auto to_delete = detail::thread_local_destructors;
  detail::thread_local_destructors = nullptr;
  delete to_delete;
  CHECK(detail::thread_local_destructors == nullptr);
}
void set_thread_id(int32 id) {
  detail::thread_id_ = id;
}

int32 get_thread_id() {
  return detail::thread_id_;
}
}  // namespace td
