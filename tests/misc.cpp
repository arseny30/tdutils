#include "td/utils/base64.h"
#include "td/utils/crypto.h"
#include "td/utils/misc.h"
#include "td/utils/port/EventFd.h"
#include "td/utils/port/FileFd.h"
#include "td/utils/port/path.h"
#include "td/utils/port/Stat.h"
#include "td/utils/port/thread.h"
#include "td/utils/Random.h"
#include "td/utils/tests.h"

#include <atomic>

#if !TD_WINDOWS
#include <unistd.h>
#endif

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
    usleep(Random::fast(0, 1000));
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
  usleep(5000000);
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

#if TD_EMSCRIPTEN
  // Nor threads, nor EventFd are supported in emscrpiten
  return;
#endif

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
}

static string rand_string(int from, int to, int len) {
  string res(len, '\0');
  for (int i = 0; i < len; i++) {
    res[i] = static_cast<char>(Random::fast(from, to));
  }
  return res;
}

static std::vector<string> rand_split(string str) {
  std::vector<string> res;
  size_t pos = 0;
  while (pos < str.size()) {
    size_t len;
    if (Random::fast(0, 1)) {
      len = Random::fast(1, 10);
    } else {
      len = Random::fast(100, 200);
    }
    if (len > str.size() - pos) {
      len = str.size() - pos;
    }
    res.push_back(str.substr(pos, len));
    pos += len;
  }
  return res;
}

TEST(Misc, Sha256) {
  string s = rand_string(0, 255, 10000);
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

TEST(Misc, base64) {
  for (int l = 0; l < 300000; l += l / 20 + 1) {
    for (int t = 0; t < 10; t++) {
      string s = rand_string(0, 255, l);
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