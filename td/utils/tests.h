#pragma once
#include "td/utils/common.h"
#include "td/utils/format.h"
#include "td/utils/List.h"
#include "td/utils/logging.h"
#include "td/utils/misc.h"
#include "td/utils/Slice.h"
#include "td/utils/Time.h"

#define REGISTER_TESTS(x)                \
  void TD_CONCAT(register_tests_, x)() { \
  }
#define DESC_TESTS(x) void TD_CONCAT(register_tests_, x)()
#define LOAD_TESTS(x) TD_CONCAT(register_tests_, x)()

namespace td {

class Test : private ListNode {
 public:
  explicit Test(CSlice name) : name_(name) {
    get_tests_list()->put_back(this);
  }

  Test(const Test &) = delete;
  Test &operator=(const Test &) = delete;
  Test(Test &&) = delete;
  Test &operator=(Test &&) = delete;
  virtual ~Test() = default;

  static void add_substr_filter(std::string str) {
    get_substr_filters()->push_back(std::move(str));
  }
  static void run_all() {
    for (auto end = get_tests_list(), cur = end->next; cur != end; cur = cur->next) {
      auto test = static_cast<td::Test *>(cur);
      bool ok = true;
      for (const auto &filter : *get_substr_filters()) {
        if (test->name_.str().find(filter) == std::string::npos) {
          ok = false;
          break;
        }
      }
      if (!ok) {
        continue;
      }

      LOG(ERROR) << "Run test " << tag("name", test->name_);
      auto start = Time::now();
      test->run();
      LOG(ERROR) << format::as_time(Time::now() - start);
    }
  }

 private:
  CSlice name_;
  static std::vector<std::string> *get_substr_filters() {
    static std::vector<std::string> substr_filters_;
    return &substr_filters_;
  }

  static ListNode *get_tests_list() {
    static ListNode root;
    return &root;
  }
  static bool &get_ok_flag() {
    static bool is_ok = true;
    return is_ok;
  }
  virtual void run() = 0;
};

template <class T1, class T2>
void ASSERT_EQ(const T1 &a, const T2 &b) {
  CHECK(a == b) << tag("expected", a) << tag("got", b);
}

template <class T>
void ASSERT_TRUE(const T &a) {
  CHECK(a);
}

template <class T1, class T2>
void ASSERT_STREQ(const T1 &a, const T2 &b) {
  ASSERT_EQ(Slice(a), Slice(b));
}

}  // namespace td

#define TEST_NAME(a, b) TD_CONCAT(Test, TD_CONCAT(a, b))

#define TEST(test_case_name, test_name) TEST_IMPL(TEST_NAME(test_case_name, test_name))

#define TEST_IMPL(test_name)                                                       \
  class test_name : public ::td::Test {                                            \
   public:                                                                         \
    using Test::Test;                                                              \
    void run() final;                                                              \
  };                                                                               \
  test_name TD_CONCAT(test_instance_, TD_CONCAT(test_name, __LINE__))(#test_name); \
  void test_name::run()
