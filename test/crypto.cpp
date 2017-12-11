#include "td/utils/base64.h"
#include "td/utils/common.h"
#include "td/utils/crypto.h"
#include "td/utils/Slice.h"
#include "td/utils/tests.h"

#include <limits>

static td::vector<td::string> strings{"", "1", "short test string", td::string(1000000, 'a')};

#if TD_HAVE_OPENSSL
TEST(Crypto, AesCtrState) {
  td::vector<td::uint32> answers1{0,         1141589763, 596296607,  3673001485, 2302125528,
                                  330967191, 2047392231, 3537459563, 307747798,  2149598133};
  td::vector<td::uint32> answers2{0,         2053451992, 1384063362, 3266188502, 2893295118,
                                  780356167, 1904947434, 2043402406, 472080809,  1807109488};

  std::size_t i = 0;
  for (auto length : {0, 1, 31, 32, 33, 9999, 10000, 10001, 999999, 1000001}) {
    td::uint32 seed = length;
    td::string s(length, '\0');
    for (auto &c : s) {
      seed = seed * 123457567u + 987651241u;
      c = static_cast<char>((seed >> 23) & 255);
    }

    td::UInt256 key;
    for (auto &c : key.raw) {
      seed = seed * 123457567u + 987651241u;
      c = (seed >> 23) & 255;
    }
    td::UInt128 iv;
    for (auto &c : iv.raw) {
      seed = seed * 123457567u + 987651241u;
      c = (seed >> 23) & 255;
    }

    td::AesCtrState state;
    state.init(key, iv);
    td::string t(length, '\0');
    state.encrypt(s, t);
    ASSERT_EQ(answers1[i], td::crc32(t));
    state.init(key, iv);
    state.decrypt(t, t);
    ASSERT_STREQ(s, t);

    for (auto &c : iv.raw) {
      c = 0xFF;
    }
    state.init(key, iv);
    state.encrypt(s, t);
    ASSERT_EQ(answers2[i], td::crc32(t));

    i++;
  }
}

TEST(Crypto, Sha256State) {
  for (auto length : {0, 1, 31, 32, 33, 9999, 10000, 10001, 999999, 1000001}) {
    auto s = td::rand_string(std::numeric_limits<char>::min(), std::numeric_limits<char>::max(), length);
    td::UInt256 baseline;
    td::sha256(s, td::MutableSlice(baseline.raw, 32));

    td::Sha256State state;
    td::sha256_init(&state);
    auto v = td::rand_split(s);
    for (auto &x : v) {
      td::sha256_update(x, &state);
    }
    td::UInt256 result;
    td::sha256_final(&state, td::MutableSlice(result.raw, 32));
    ASSERT_TRUE(baseline == result);
  }
}

TEST(Crypto, PBKDF) {
  td::vector<td::string> passwords{"", "qwerty", std::string(1000, 'a')};
  td::vector<td::string> salts{"", "qwerty", std::string(1000, 'a')};
  td::vector<int> iteration_counts{1, 2, 1000};
  td::vector<td::Slice> answers{
      "984LZT0tcqQQjPWr6RL/3Xd2Ftu7J6cOggTzri0Pb60=", "lzmEEdaupDp3rO+SImq4J41NsGaL0denanJfdoCsRcU=",
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

  std::size_t pos = 0;
  for (auto &password : passwords) {
    for (auto &salt : salts) {
      for (auto &iteration_count : iteration_counts) {
        char result[32];
        td::pbkdf2_sha256(password, salt, iteration_count, {result, 32});
        ASSERT_STREQ(answers[pos], td::base64_encode({result, 32}));
        pos++;
      }
    }
  }
}

TEST(Crypto, sha1) {
  td::vector<td::Slice> answers{"2jmj7l5rSw0yVb/vlWAYkK/YBwk=", "NWoZK3kTsExUV00Ywo1G5jlUKKs=",
                                "uRysQwoax0pNJeBC3+zpQzJy1rA=", "NKqXPNTE2qT2Husr260nMWU0AW8="};

  for (std::size_t i = 0; i < strings.size(); i++) {
    unsigned char output[20];
    td::sha1(strings[i], output);
    ASSERT_STREQ(answers[i], td::base64_encode(td::Slice(output, 20)));
  }
}

TEST(Crypto, sha256) {
  td::vector<td::Slice> answers{
      "47DEQpj8HBSa+/TImW+5JCeuQeRkm5NMpJWZG3hSuFU=", "a4ayc/80/OGda4BO/1o/V0etpOqiLx1JwB5S3beHW0s=",
      "yPMaY7Q8PKPwCsw64UnDD5mhRcituEJgzLZMvr0O8pY=", "zcduXJkU+5KBocfihNc+Z/GAmkiklyAOBG05zMcRLNA="};

  for (std::size_t i = 0; i < strings.size(); i++) {
    td::string output(32, '\0');
    td::sha256(strings[i], output);
    ASSERT_STREQ(answers[i], td::base64_encode(output));
  }
}

TEST(Crypto, md5) {
  td::vector<td::Slice> answers{
      "1B2M2Y8AsgTpgAmY7PhCfg==", "xMpCOKC5I4INzFCab3WEmw==", "vwBninYbDRkgk+uA7GMiIQ==", "dwfWrk4CfHDuoqk1wilvIQ=="};

  for (std::size_t i = 0; i < strings.size(); i++) {
    td::string output(16, '\0');
    td::md5(strings[i], output);
    ASSERT_STREQ(answers[i], td::base64_encode(output));
  }
}
#endif

#if TD_HAVE_ZLIB
TEST(Crypto, crc32) {
  td::vector<td::uint32> answers{0, 2212294583, 3013144151, 3693461436};

  for (std::size_t i = 0; i < strings.size(); i++) {
    ASSERT_EQ(answers[i], td::crc32(strings[i]));
  }
}
#endif

TEST(Crypto, crc64) {
  td::vector<td::uint64> answers{0, 3039664240384658157, 17549519902062861804u, 8794730974279819706};

  for (std::size_t i = 0; i < strings.size(); i++) {
    ASSERT_EQ(answers[i], td::crc64(strings[i]));
  }
}
