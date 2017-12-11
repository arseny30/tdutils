#include "td/utils/base64.h"
#include "td/utils/crypto.h"
#include "td/utils/logging.h"
#include "td/utils/misc.h"
#include "td/utils/port/EventFd.h"
#include "td/utils/port/FileFd.h"
#include "td/utils/port/path.h"
#include "td/utils/port/sleep.h"
#include "td/utils/port/Stat.h"
#include "td/utils/port/thread.h"
#include "td/utils/Random.h"
#include "td/utils/Slice.h"
#include "td/utils/tests.h"

#include <atomic>
#include <limits>

using namespace td;

#if TD_LINUX || TD_DARWIN
TEST(Misc, update_atime_saves_mtime) {
  SET_VERBOSITY_LEVEL(VERBOSITY_NAME(ERROR));
  std::string name = "test_file";
  auto r_file = FileFd::open(name, FileFd::Read | FileFd::Flags::Create | FileFd::Flags::Truncate);
  LOG_IF(ERROR, r_file.is_error()) << r_file.error();
  ASSERT_TRUE(r_file.is_ok());
  r_file.move_as_ok().close();

  auto info = stat(name).ok();
  int32 tests_ok = 0;
  int32 tests_wa = 0;
  for (int i = 0; i < 10000; i++) {
    update_atime(name).ensure();
    auto new_info = stat(name).ok();
    if (info.mtime_nsec_ == new_info.mtime_nsec_) {
      tests_ok++;
    } else {
      tests_wa++;
      info.mtime_nsec_ = new_info.mtime_nsec_;
    }
    ASSERT_EQ(info.mtime_nsec_, new_info.mtime_nsec_);
    usleep_for(Random::fast(0, 1000));
  }
  if (tests_wa > 0) {
    LOG(ERROR) << "Access time was unexpectedly updated " << tests_wa << " times";
  }
  unlink(name).ensure();
}

TEST(Misc, update_atime_change_atime) {
  SET_VERBOSITY_LEVEL(VERBOSITY_NAME(ERROR));
  std::string name = "test_file";
  auto r_file = FileFd::open(name, FileFd::Read | FileFd::Flags::Create | FileFd::Flags::Truncate);
  LOG_IF(ERROR, r_file.is_error()) << r_file.error();
  ASSERT_TRUE(r_file.is_ok());
  r_file.move_as_ok().close();
  auto info = stat(name).ok();
  // not enough for fat and e.t.c.
  usleep_for(5000000);
  update_atime(name).ensure();
  auto new_info = stat(name).ok();
  if (info.atime_nsec_ == new_info.atime_nsec_) {
    LOG(ERROR) << "Access time was unexpectedly not changed";
  }
  unlink(name).ensure();
}
#endif

TEST(Misc, errno_tls_bug) {
  // That's a problem that should be avoided
  // errno = 0;
  // impl_.alloc(123);
  // CHECK(errno == 0);

#if !TD_THREAD_UNSUPPORTED && !TD_EVENTFD_UNSUPPORTED
  EventFd test_event_fd;
  test_event_fd.init();
  std::atomic<int> s(0);
  s = 1;
  td::thread th([&] {
    while (s != 1) {
    }
    test_event_fd.acquire();
  });
  th.join();

  for (int i = 0; i < 1000; i++) {
    vector<EventFd> events(10);
    vector<td::thread> threads;
    for (auto &event : events) {
      event.init();
      event.release();
    }
    for (auto &event : events) {
      threads.push_back(td::thread([&] {
        {
          EventFd tmp;
          tmp.init();
          tmp.acquire();
        }
        event.acquire();
      }));
    }
    for (auto &thread : threads) {
      thread.join();
    }
  }
#endif
}

#if TD_HAVE_OPENSSL
TEST(Misc, Sha256) {
  string s = rand_string(std::numeric_limits<char>::min(), std::numeric_limits<char>::max(), 10000);
  UInt256 baseline;
  sha256(s, MutableSlice(baseline.raw, 32));

  Sha256State state;
  sha256_init(&state);
  auto v = rand_split(s);
  for (auto &x : v) {
    sha256_update(x, &state);
  }
  UInt256 result;
  sha256_final(&state, MutableSlice(result.raw, 32));
  ASSERT_TRUE(baseline == result);
}
#endif

TEST(Misc, base64) {
  for (int l = 0; l < 300000; l += l / 20 + 1) {
    for (int t = 0; t < 10; t++) {
      string s = rand_string(std::numeric_limits<char>::min(), std::numeric_limits<char>::max(), l);
      string encoded = base64url_encode(s);
      auto decoded = base64url_decode(encoded);
      ASSERT_TRUE(decoded.is_ok());
      ASSERT_TRUE(decoded.ok() == s);

      encoded = base64_encode(s);
      decoded = base64_decode(encoded);
      ASSERT_TRUE(decoded.is_ok());
      ASSERT_TRUE(decoded.ok() == s);
    }
  }

  ASSERT_TRUE(base64url_decode("dGVzdA").is_ok());
  ASSERT_TRUE(base64url_decode("dGVzdB").is_error());
  ASSERT_TRUE(base64_encode(base64url_decode("dGVzdA").ok()) == "dGVzdA==");
  ASSERT_TRUE(base64_encode("any carnal pleas") == "YW55IGNhcm5hbCBwbGVhcw==");
  ASSERT_TRUE(base64_encode("any carnal pleasu") == "YW55IGNhcm5hbCBwbGVhc3U=");
  ASSERT_TRUE(base64_encode("any carnal pleasur") == "YW55IGNhcm5hbCBwbGVhc3Vy");
  ASSERT_TRUE(base64_encode("      /'.;.';≤.];,].',[.;/,.;/]/..;!@#!*(%?::;!%\";") ==
              "ICAgICAgLycuOy4nO+KJpC5dOyxdLicsWy47LywuOy9dLy4uOyFAIyEqKCU/"
              "Ojo7ISUiOw==");
}

TEST(Misc, to_integer) {
  ASSERT_EQ(to_integer<int32>("-1234567"), -1234567);
  ASSERT_EQ(to_integer<int64>("-1234567"), -1234567);
  ASSERT_EQ(to_integer<uint32>("-1234567"), 0u);
  ASSERT_EQ(to_integer<int16>("-1234567"), 10617);
  ASSERT_EQ(to_integer<uint16>("-1234567"), 0u);
  ASSERT_EQ(to_integer<int16>("-1254567"), -9383);
  ASSERT_EQ(to_integer<uint16>("1254567"), 9383u);
  ASSERT_EQ(to_integer<int64>("-12345678910111213"), -12345678910111213);
  ASSERT_EQ(to_integer<uint64>("12345678910111213"), 12345678910111213ull);

  ASSERT_EQ(to_integer_safe<int32>("-1234567").ok(), -1234567);
  ASSERT_EQ(to_integer_safe<int64>("-1234567").ok(), -1234567);
  ASSERT_TRUE(to_integer_safe<uint32>("-1234567").is_error());
  ASSERT_TRUE(to_integer_safe<int16>("-1234567").is_error());
  ASSERT_TRUE(to_integer_safe<uint16>("-1234567").is_error());
  ASSERT_TRUE(to_integer_safe<int16>("-1254567").is_error());
  ASSERT_TRUE(to_integer_safe<uint16>("1254567").is_error());
  ASSERT_EQ(to_integer_safe<int64>("-12345678910111213").ok(), -12345678910111213);
  ASSERT_EQ(to_integer_safe<uint64>("12345678910111213").ok(), 12345678910111213ull);
  ASSERT_TRUE(to_integer_safe<uint64>("-12345678910111213").is_error());
}

TEST(Misc, PBKDF) {
  vector<td::string> passwords{"", "qwerty", std::string(1000, 'a')};
  vector<td::string> salts{"", "qwerty", std::string(1000, 'a')};
  vector<int> iteration_counts{1, 2, 1000};
  vector<Slice> answers{"984LZT0tcqQQjPWr6RL/3Xd2Ftu7J6cOggTzri0Pb60=", "lzmEEdaupDp3rO+SImq4J41NsGaL0denanJfdoCsRcU=",
                        "T8WKIcEAzhg1uPmZHXOLVpZdFLJOF2H73/xprF4LZno=", "NHxAnMhPOATsb1wV0cGDlAIs+ofzI6I4I8eGJeWN9Qw=",
                        "fjYi7waEPjbVYEuZ61/Nm2hbk/vRdShoJoXg4Ygnqe4=", "GhW6e95hGJSf+ID5IrSbvzWyBZ1l35A+UoL55Uh/njk=",
                        "BueLDpqSCEc0GWk83WgMwz3UsWwfvVKcvllETSB/Yq8=", "hgHgJZNWRh78PyPdVJsK8whgHOHQbNQiyaTuGDX2IFo=",
                        "T2xdyNT1GlcA4+MVNzOe7NCgSAAzNkanNsmuoSr+4xQ=", "/f6t++GUPE+e63+0TrlInL+UsmzRSAAFopa8BBBmb2w=",
                        "8Zn98QEAKS9wPOUlN09+pfm0SWs1IGeQxQkNMT/1k48=", "sURLQ/6UX/KVYedyQB21oAtMJ+STZ4iwpxfQtqmWkLw=",
                        "T9t/EJXFpPs2Lhca7IVGphTC/OdEloPMHw1UhDnXcyQ=", "TIrtN05E9KQL6Lp/wjtbsFS+KkWZ8jlGK0ErtaoitOg=",
                        "+1KcMBjyUNz5VMaIfE5wkGwS6I+IQ5FhK+Ou2HgtVoQ=", "h36ci1T0vGllCl/xJxq6vI7n28Bg40dilzWOKg6Jt8k=",
                        "9uwsHJsotTiTqqCYftN729Dg7QI2BijIjV2MvSEUAeE=", "/l+vd/XYgbioh1SfLMaGRr13udmY6TLSlG4OYmytwGU=",
                        "7qfZZBbMRLtgjqq7GHgWa/UfXPajW8NXpJ6/T3P1rxI=", "ufwz94p28WnoOFdbrb1oyQEzm/v0CV2b0xBVxeEPJGA=",
                        "T/PUUBX2vGMUsI6httlhbMHlGPMvqFBNzayU5voVlaw=", "viMvsvTg9GfQymF3AXZ8uFYTDa3qLrqJJk9w/74iZfg=",
                        "HQF+rOZMW4DAdgZz8kAMe28eyIi0rs3a3u/mUeGPNfs=", "7lBVA+GnSxWF/eOo+tyyTB7niMDl1MqP8yzo+xnHTyw=",
                        "aTWb7HQAxaTKhSiRPY3GuM1GVmq/FPuwWBU/TUpdy70=", "fbg8M/+Ht/oU+UAZ4dQcGPo+wgCCHaA+GM4tm5jnWcY=",
                        "DJbCGFMIR/5neAlpda8Td5zftK4NGekVrg2xjrKW/4c="};

  size_t pos = 0;
  for (auto &password : passwords) {
    for (auto &salt : salts) {
      for (auto &iteration_count : iteration_counts) {
        char result[32];
        pbkdf2_sha256(password, salt, iteration_count, {result, 32});
        ASSERT_STREQ(answers[pos], base64_encode({result, 32}));
        pos++;
      }
    }
  }
}
