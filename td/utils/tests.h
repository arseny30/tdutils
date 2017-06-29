#pragma once
#include "td/utils/common.h"
#include "td/utils/format.h"
#include "td/utils/logging.h"
#include "td/utils/misc.h"
#include "td/utils/Slice.h"

#define REGISTER_TESTS(x)                \
  void TD_CONCAT(register_tests_, x)() { \
  }
#define DESC_TESTS(x) void TD_CONCAT(register_tests_, x)()
#define LOAD_TESTS(x) TD_CONCAT(register_tests_, x)()

namespace td {

class Test : private ListNode {
 public:
  virtual ~Test() = default;
  static void run_all() {
    for (auto end = get_tests_list(), cur = end->next; cur != end; cur = cur->next) {
      auto test = static_cast<td::Test *>(cur);
      LOG(ERROR) << "Run test " << tag("name", test->name_);
      test->run();
    }
  }
  explicit Test(CSlice name) : name_(name) {
    get_tests_list()->put_back(this);
  }

 private:
  CSlice name_;
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
}  // namespace td

#define HasNonfatalFailure() false

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

#define EXPECT_EQ(a, b) CHECK(a == b) << tag("expected", a) << tag("got", b)
#define ASSERT_EQ(a, b) CHECK(a == b) << tag("expected", a) << tag("got", b)
#define EXPECT_TRUE(a) CHECK(a)
#define ASSERT_TRUE(a) CHECK(a)
#define EXPECT_STREQ(a, b) EXPECT_EQ(Slice(a), Slice(b))
#define ASSERT_STREQ(a, b) ASSERT_EQ(Slice(a), Slice(b))
