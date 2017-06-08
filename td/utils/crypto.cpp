#include "td/utils/crypto.h"

#include "td/utils/logging.h"
#include "td/utils/misc.h"
#include "td/utils/port/RwMutex.h"
#include "td/utils/port/thread.h"
#include "td/utils/Random.h"

#include <openssl/aes.h>
#include <openssl/bn.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/md5.h>
#include <openssl/sha.h>
#include <openssl/hmac.h>

#include <zlib.h>  // for crc32

#include <algorithm>
#include <cstring>
#include <utility>

namespace td {
static string BN_to_string(const BIGNUM *bn, int size = 0) {
  int num_size = BN_num_bytes(bn);
  if (size == 0) {
    size = num_size;
  } else {
    CHECK(size >= num_size);
  }
  string res(size, 0);
  BN_bn2bin(bn, reinterpret_cast<unsigned char *>(&res[size - num_size]));
  return res;
}

template <class FromT>
static string as_big_endian_string(const FromT &from) {
  size_t size = sizeof(from);
  string res(size, '\0');

  auto ptr = reinterpret_cast<const unsigned char *>(&from);
  std::memcpy(&res[0], ptr, size);

  size_t i = size;
  while (i && res[i - 1] == 0) {
    i--;
  }

  res.resize(i);
  std::reverse(res.begin(), res.end());
  return res;
}

// static void BN_print(Slice name, BIGNUM *num) {
// char *p_dec = BN_bn2dec(num);
// LOG(INFO) << tag(name, p_dec);
// OPENSSL_free(p_dec);
//}

static uint64 gcd(uint64 a, uint64 b) {
  if (a == 0) {
    return b;
  }
  if (b == 0) {
    return a;
  }

  int shift = 0;
  while ((a & 1) == 0 && (b & 1) == 0) {
    a >>= 1;
    b >>= 1;
    shift++;
  }

  while (true) {
    while ((a & 1) == 0) {
      a >>= 1;
    }
    while ((b & 1) == 0) {
      b >>= 1;
    }
    if (a > b) {
      a -= b;
    } else if (b > a) {
      b -= a;
    } else {
      return a << shift;
    }
  }
}

uint64 pq_factorize(uint64 what) {
  if (what < 2 || what > (static_cast<uint64>(1) << 63)) {
    return 1;
  }
  uint64 g = 0;
  for (int i = 0, it = 0; i < 3 || it < 1000; i++) {
    uint64 q = Random::fast(17, 32) % (what - 1);
    uint64 x = Random::fast_uint64() % (what - 1) + 1;
    uint64 y = x;
    int lim = 1 << (std::min(5, i) + 18);
    for (int j = 1; j < lim; j++) {
      it++;
      uint64 a = x;
      uint64 b = x;
      uint64 c = q;

      // c += a * b
      while (b) {
        if (b & 1) {
          c += a;
          if (c >= what) {
            c -= what;
          }
        }
        a += a;
        if (a >= what) {
          a -= what;
        }
        b >>= 1;
      }

      x = c;
      uint64 z = x < y ? what + x - y : x - y;
      g = gcd(z, what);
      if (g != 1) {
        break;
      }

      if (!(j & (j - 1))) {
        y = x;
      }
    }
    if (g > 1 && g < what) {
      break;
    }
  }
  if (g != 0) {
    uint64 other = what / g;
    if (other < g) {
      g = other;
    }
  }
  return g;
}

static int pq_factorize_big(Slice pq_str, string *p_str, string *q_str) {
  // TODO: qsieve?
  // do not work for pq == 1
  BN_CTX *ctx = BN_CTX_new();
  BIGNUM *pq = BN_new();
  BIGNUM *a = BN_new();
  BIGNUM *b = BN_new();

  BIGNUM *p = BN_new();
  BIGNUM *q = BN_new();
  BIGNUM *one = BN_new();
  BN_one(one);

  BN_bin2bn(pq_str.ubegin(), static_cast<int>(pq_str.size()), pq);

  bool found = false;
  for (int i = 0, it = 0; !found && (i < 3 || it < 1000); i++) {
    int32 t = Random::fast(17, 32);
    BN_set_word(a, Random::fast_uint32());
    BN_copy(b, a);

    int32 lim = 1 << (i + 23);
    for (int j = 1; j < lim; j++) {
      it++;
      BN_mod_mul(a, a, a, pq, ctx);
      BN_add_word(a, t);
      if (BN_cmp(a, b) > 0) {
        BN_sub(q, a, b);
      } else {
        BN_sub(q, b, a);
      }
      BN_gcd(p, q, pq, ctx);
      if (BN_cmp(p, one) != 0) {
        found = true;
        break;
      }
      if ((j & (j - 1)) == 0) {
        BN_copy(b, a);
      }
    }
  }

  int result = -1;
  if (found) {
    result = 0;

    BN_div(q, nullptr, pq, p, ctx);
    if (BN_cmp(p, q) > 0) {
      std::swap(p, q);
    }

    *p_str = BN_to_string(p);
    *q_str = BN_to_string(q);
  }

  BN_free(one);
  BN_free(p);
  BN_free(q);
  BN_free(b);
  BN_free(a);
  BN_CTX_free(ctx);
  return result;
}

int pq_factorize(Slice pq_str, string *p_str, string *q_str) {
  size_t size = pq_str.size();
  if (static_cast<int>(size) > 8 || (static_cast<int>(size) == 8 && (pq_str.begin()[0] & 128) != 0)) {
    int res = pq_factorize_big(pq_str, p_str, q_str);
    return res;
  }

  auto ptr = pq_str.ubegin();
  uint64 pq = 0;
  for (int i = 0; i < static_cast<int>(size); i++) {
    pq = (pq << 8) | ptr[i];
  }

  uint64 p = pq_factorize(pq);
  if (p == 0 || pq % p != 0) {
    return -1;
  }
  *p_str = as_big_endian_string(p);
  *q_str = as_big_endian_string(pq / p);

  // std::string p2, q2;
  // pq_factorize_big(pq_str, &p2, &q2);
  // CHECK(*p_str == p2);
  // CHECK(*q_str == q2);
  return 0;
}

/*** AES ***/
ssize_t aes_ige_xcrypt(const UInt256 &aes_key, UInt256 *aes_iv, Slice from, MutableSlice to, bool encrypt_flag) {
  AES_KEY key;
  if (encrypt_flag) {
    AES_set_encrypt_key(aes_key.raw, 256, &key);
  } else {
    AES_set_decrypt_key(aes_key.raw, 256, &key);
  }
  CHECK(from.size() <= to.size());
  AES_ige_encrypt(from.ubegin(), to.ubegin(), from.size(), &key, aes_iv->raw, encrypt_flag);
  return from.size();
}

ssize_t aes_ige_encrypt(const UInt256 &aes_key, UInt256 *aes_iv, Slice from, MutableSlice to) {
  return aes_ige_xcrypt(aes_key, aes_iv, from, to, true);
}

ssize_t aes_ige_decrypt(const UInt256 &aes_key, UInt256 *aes_iv, Slice from, MutableSlice to) {
  return aes_ige_xcrypt(aes_key, aes_iv, from, to, false);
}

#if OPENSSL_VERSION_NUMBER < 0x10100000L
struct AesCtrStateImpl {
 public:
  AesCtrStateImpl() = default;
  AesCtrStateImpl(const AesCtrStateImpl &) = delete;
  AesCtrStateImpl &operator=(const AesCtrStateImpl &) = delete;
  AesCtrStateImpl(AesCtrStateImpl &&other) = delete;
  AesCtrStateImpl &operator=(AesCtrStateImpl &&other) = delete;
  ~AesCtrStateImpl() {
    EVP_CIPHER_CTX_cleanup(&ctx_inner_);
  }
  EVP_CIPHER_CTX *get() {
    return &ctx_inner_;
  }

 private:
  EVP_CIPHER_CTX ctx_inner_;
};
#else
struct AesCtrStateImpl {
 public:
  AesCtrStateImpl() : ctx_(EVP_CIPHER_CTX_new(), &EVP_CIPHER_CTX_free) {
  }
  EVP_CIPHER_CTX *get() {
    return ctx_.get();
  }

 private:
  std::unique_ptr<EVP_CIPHER_CTX, void (*)(EVP_CIPHER_CTX *)> ctx_;
};
#endif

AesCtrState::AesCtrState() = default;
AesCtrState::~AesCtrState() = default;
AesCtrState::AesCtrState(AesCtrState &&from) = default;
AesCtrState &AesCtrState::operator=(AesCtrState &&from) = default;

void init_aes_ctr_state(const UInt256 &key, const UInt128 &iv, AesCtrState *state) {
  state->ctx_ = std::make_unique<AesCtrStateImpl>();
  int error;
  error = EVP_EncryptInit_ex(state->ctx_->get(), EVP_aes_256_ctr(), nullptr, key.raw, iv.raw);
  CHECK(error);
  error = EVP_CIPHER_CTX_set_padding(state->ctx_->get(), 0);

  // LOG(ERROR) << EVP_CIPHER_CTX_block_size(state->ctx_->get());
  // LOG(ERROR) << EVP_CIPHER_CTX_key_length(state->ctx_->get());
  // LOG(ERROR) << EVP_CIPHER_CTX_iv_length(state->ctx_->get());
  CHECK(error);
}

ssize_t aes_ctr_xcrypt(AesCtrState *state, Slice from, MutableSlice to, bool encrypt_flag) {
  int from_size = narrow_cast<int>(from.size());
  int to_size = narrow_cast<int>(to.size());
  auto error = EVP_EncryptUpdate(state->ctx_->get(), to.ubegin(), &to_size, from.ubegin(), from_size);
  CHECK(error);
  CHECK(to_size == from_size);
  return 0;
}
ssize_t aes_ctr_encrypt(AesCtrState *state, Slice from, MutableSlice to) {
  return aes_ctr_xcrypt(state, from, to, true);
}
ssize_t aes_ctr_decrypt(AesCtrState *state, Slice from, MutableSlice to) {
  return aes_ctr_xcrypt(state, from, to, false);
}

ssize_t aes_cbc_xcrypt(const UInt256 &aes_key, UInt128 *aes_iv, Slice from, MutableSlice to, bool encrypt_flag) {
  AES_KEY key;
  if (encrypt_flag) {
    AES_set_encrypt_key(aes_key.raw, 256, &key);
  } else {
    AES_set_decrypt_key(aes_key.raw, 256, &key);
  }
  CHECK(from.size() <= to.size());
  AES_cbc_encrypt(from.ubegin(), to.ubegin(), from.size(), &key, aes_iv->raw, encrypt_flag);
  return from.size();
}

ssize_t aes_cbc_encrypt(const UInt256 &aes_key, UInt128 *aes_iv, Slice from, MutableSlice to) {
  return aes_cbc_xcrypt(aes_key, aes_iv, from, to, true);
}

ssize_t aes_cbc_decrypt(const UInt256 &aes_key, UInt128 *aes_iv, Slice from, MutableSlice to) {
  return aes_cbc_xcrypt(aes_key, aes_iv, from, to, false);
}

/*** SHA-1 ***/
void sha1(Slice input, unsigned char output[20]) {
  SHA1(input.ubegin(), input.size(), output);
}

void sha256(Slice input, MutableSlice output) {
  CHECK(output.size() >= 32);
  SHA256(input.ubegin(), input.size(), output.ubegin());
}

struct Sha256StateImpl {
  SHA256_CTX ctx;
};

Sha256State::Sha256State() = default;
Sha256State::Sha256State(Sha256State &&from) = default;
Sha256State &Sha256State::operator=(Sha256State &&from) = default;
Sha256State::~Sha256State() = default;

void sha256_init(Sha256State *state) {
  state->impl = std::make_unique<Sha256StateImpl>();
  SHA256_Init(&state->impl->ctx);
}
void sha256_update(Slice data, Sha256State *state) {
  CHECK(state->impl);
  SHA256_Update(&state->impl->ctx, data.ubegin(), data.size());
}
void sha256_final(Sha256State *state, MutableSlice output) {
  CHECK(output.size() >= 32);
  CHECK(state->impl);
  SHA256_Final(output.ubegin(), &state->impl->ctx);
  state->impl.reset();
}

/*** md5 ***/
void md5(Slice input, MutableSlice output) {
  CHECK(output.size() >= MD5_DIGEST_LENGTH);
  MD5(input.ubegin(), input.size(), output.ubegin());
}

static const uint64 crc64_table[256] = {
    0x0000000000000000, 0xb32e4cbe03a75f6f, 0xf4843657a840a05b, 0x47aa7ae9abe7ff34, 0x7bd0c384ff8f5e33,
    0xc8fe8f3afc28015c, 0x8f54f5d357cffe68, 0x3c7ab96d5468a107, 0xf7a18709ff1ebc66, 0x448fcbb7fcb9e309,
    0x0325b15e575e1c3d, 0xb00bfde054f94352, 0x8c71448d0091e255, 0x3f5f08330336bd3a, 0x78f572daa8d1420e,
    0xcbdb3e64ab761d61, 0x7d9ba13851336649, 0xceb5ed8652943926, 0x891f976ff973c612, 0x3a31dbd1fad4997d,
    0x064b62bcaebc387a, 0xb5652e02ad1b6715, 0xf2cf54eb06fc9821, 0x41e11855055bc74e, 0x8a3a2631ae2dda2f,
    0x39146a8fad8a8540, 0x7ebe1066066d7a74, 0xcd905cd805ca251b, 0xf1eae5b551a2841c, 0x42c4a90b5205db73,
    0x056ed3e2f9e22447, 0xb6409f5cfa457b28, 0xfb374270a266cc92, 0x48190ecea1c193fd, 0x0fb374270a266cc9,
    0xbc9d3899098133a6, 0x80e781f45de992a1, 0x33c9cd4a5e4ecdce, 0x7463b7a3f5a932fa, 0xc74dfb1df60e6d95,
    0x0c96c5795d7870f4, 0xbfb889c75edf2f9b, 0xf812f32ef538d0af, 0x4b3cbf90f69f8fc0, 0x774606fda2f72ec7,
    0xc4684a43a15071a8, 0x83c230aa0ab78e9c, 0x30ec7c140910d1f3, 0x86ace348f355aadb, 0x3582aff6f0f2f5b4,
    0x7228d51f5b150a80, 0xc10699a158b255ef, 0xfd7c20cc0cdaf4e8, 0x4e526c720f7dab87, 0x09f8169ba49a54b3,
    0xbad65a25a73d0bdc, 0x710d64410c4b16bd, 0xc22328ff0fec49d2, 0x85895216a40bb6e6, 0x36a71ea8a7ace989,
    0x0adda7c5f3c4488e, 0xb9f3eb7bf06317e1, 0xfe5991925b84e8d5, 0x4d77dd2c5823b7ba, 0x64b62bcaebc387a1,
    0xd7986774e864d8ce, 0x90321d9d438327fa, 0x231c512340247895, 0x1f66e84e144cd992, 0xac48a4f017eb86fd,
    0xebe2de19bc0c79c9, 0x58cc92a7bfab26a6, 0x9317acc314dd3bc7, 0x2039e07d177a64a8, 0x67939a94bc9d9b9c,
    0xd4bdd62abf3ac4f3, 0xe8c76f47eb5265f4, 0x5be923f9e8f53a9b, 0x1c4359104312c5af, 0xaf6d15ae40b59ac0,
    0x192d8af2baf0e1e8, 0xaa03c64cb957be87, 0xeda9bca512b041b3, 0x5e87f01b11171edc, 0x62fd4976457fbfdb,
    0xd1d305c846d8e0b4, 0x96797f21ed3f1f80, 0x2557339fee9840ef, 0xee8c0dfb45ee5d8e, 0x5da24145464902e1,
    0x1a083bacedaefdd5, 0xa9267712ee09a2ba, 0x955cce7fba6103bd, 0x267282c1b9c65cd2, 0x61d8f8281221a3e6,
    0xd2f6b4961186fc89, 0x9f8169ba49a54b33, 0x2caf25044a02145c, 0x6b055fede1e5eb68, 0xd82b1353e242b407,
    0xe451aa3eb62a1500, 0x577fe680b58d4a6f, 0x10d59c691e6ab55b, 0xa3fbd0d71dcdea34, 0x6820eeb3b6bbf755,
    0xdb0ea20db51ca83a, 0x9ca4d8e41efb570e, 0x2f8a945a1d5c0861, 0x13f02d374934a966, 0xa0de61894a93f609,
    0xe7741b60e174093d, 0x545a57dee2d35652, 0xe21ac88218962d7a, 0x5134843c1b317215, 0x169efed5b0d68d21,
    0xa5b0b26bb371d24e, 0x99ca0b06e7197349, 0x2ae447b8e4be2c26, 0x6d4e3d514f59d312, 0xde6071ef4cfe8c7d,
    0x15bb4f8be788911c, 0xa6950335e42fce73, 0xe13f79dc4fc83147, 0x521135624c6f6e28, 0x6e6b8c0f1807cf2f,
    0xdd45c0b11ba09040, 0x9aefba58b0476f74, 0x29c1f6e6b3e0301b, 0xc96c5795d7870f42, 0x7a421b2bd420502d,
    0x3de861c27fc7af19, 0x8ec62d7c7c60f076, 0xb2bc941128085171, 0x0192d8af2baf0e1e, 0x4638a2468048f12a,
    0xf516eef883efae45, 0x3ecdd09c2899b324, 0x8de39c222b3eec4b, 0xca49e6cb80d9137f, 0x7967aa75837e4c10,
    0x451d1318d716ed17, 0xf6335fa6d4b1b278, 0xb199254f7f564d4c, 0x02b769f17cf11223, 0xb4f7f6ad86b4690b,
    0x07d9ba1385133664, 0x4073c0fa2ef4c950, 0xf35d8c442d53963f, 0xcf273529793b3738, 0x7c0979977a9c6857,
    0x3ba3037ed17b9763, 0x888d4fc0d2dcc80c, 0x435671a479aad56d, 0xf0783d1a7a0d8a02, 0xb7d247f3d1ea7536,
    0x04fc0b4dd24d2a59, 0x3886b22086258b5e, 0x8ba8fe9e8582d431, 0xcc0284772e652b05, 0x7f2cc8c92dc2746a,
    0x325b15e575e1c3d0, 0x8175595b76469cbf, 0xc6df23b2dda1638b, 0x75f16f0cde063ce4, 0x498bd6618a6e9de3,
    0xfaa59adf89c9c28c, 0xbd0fe036222e3db8, 0x0e21ac88218962d7, 0xc5fa92ec8aff7fb6, 0x76d4de52895820d9,
    0x317ea4bb22bfdfed, 0x8250e80521188082, 0xbe2a516875702185, 0x0d041dd676d77eea, 0x4aae673fdd3081de,
    0xf9802b81de97deb1, 0x4fc0b4dd24d2a599, 0xfceef8632775faf6, 0xbb44828a8c9205c2, 0x086ace348f355aad,
    0x34107759db5dfbaa, 0x873e3be7d8faa4c5, 0xc094410e731d5bf1, 0x73ba0db070ba049e, 0xb86133d4dbcc19ff,
    0x0b4f7f6ad86b4690, 0x4ce50583738cb9a4, 0xffcb493d702be6cb, 0xc3b1f050244347cc, 0x709fbcee27e418a3,
    0x3735c6078c03e797, 0x841b8ab98fa4b8f8, 0xadda7c5f3c4488e3, 0x1ef430e13fe3d78c, 0x595e4a08940428b8,
    0xea7006b697a377d7, 0xd60abfdbc3cbd6d0, 0x6524f365c06c89bf, 0x228e898c6b8b768b, 0x91a0c532682c29e4,
    0x5a7bfb56c35a3485, 0xe955b7e8c0fd6bea, 0xaeffcd016b1a94de, 0x1dd181bf68bdcbb1, 0x21ab38d23cd56ab6,
    0x9285746c3f7235d9, 0xd52f0e859495caed, 0x6601423b97329582, 0xd041dd676d77eeaa, 0x636f91d96ed0b1c5,
    0x24c5eb30c5374ef1, 0x97eba78ec690119e, 0xab911ee392f8b099, 0x18bf525d915feff6, 0x5f1528b43ab810c2,
    0xec3b640a391f4fad, 0x27e05a6e926952cc, 0x94ce16d091ce0da3, 0xd3646c393a29f297, 0x604a2087398eadf8,
    0x5c3099ea6de60cff, 0xef1ed5546e415390, 0xa8b4afbdc5a6aca4, 0x1b9ae303c601f3cb, 0x56ed3e2f9e224471,
    0xe5c372919d851b1e, 0xa26908783662e42a, 0x114744c635c5bb45, 0x2d3dfdab61ad1a42, 0x9e13b115620a452d,
    0xd9b9cbfcc9edba19, 0x6a978742ca4ae576, 0xa14cb926613cf817, 0x1262f598629ba778, 0x55c88f71c97c584c,
    0xe6e6c3cfcadb0723, 0xda9c7aa29eb3a624, 0x69b2361c9d14f94b, 0x2e184cf536f3067f, 0x9d36004b35545910,
    0x2b769f17cf112238, 0x9858d3a9ccb67d57, 0xdff2a94067518263, 0x6cdce5fe64f6dd0c, 0x50a65c93309e7c0b,
    0xe388102d33392364, 0xa4226ac498dedc50, 0x170c267a9b79833f, 0xdcd7181e300f9e5e, 0x6ff954a033a8c131,
    0x28532e49984f3e05, 0x9b7d62f79be8616a, 0xa707db9acf80c06d, 0x14299724cc279f02, 0x5383edcd67c06036,
    0xe0ada17364673f59};

uint64 crc64_partial(Slice data, uint64 crc) {
  const char *p = data.begin();
  for (auto len = data.size(); len > 0; len--) {
    crc = crc64_table[(crc ^ *p++) & 0xff] ^ (crc >> 8);
  }
  return crc;
}

uint64 crc64(Slice data) {
  return crc64_partial(data, static_cast<uint64>(-1)) ^ static_cast<uint64>(-1);
}

uint32 crc32(Slice data) {
  auto res = static_cast<uint32>(::crc32(0, data.ubegin(), static_cast<uint32>(data.size())));
  return res;
}

void pbkdf2_sha256(Slice password, Slice salt, int iteration_count, MutableSlice dest) {
  PKCS5_PBKDF2_HMAC(password.data(), narrow_cast<int>(password.size()), salt.ubegin(), narrow_cast<int>(salt.size()),
                    iteration_count, EVP_sha256(), narrow_cast<int>(dest.size()), dest.ubegin());
}

void hmac_sha256(Slice key, Slice message, MutableSlice dest) {
  CHECK(dest.size() == 256 / 8);
  unsigned int len = 0;
  HMAC(EVP_sha256(), key.ubegin(), narrow_cast<int>(key.size()), message.ubegin(), narrow_cast<int>(message.size()),
       dest.ubegin(), &len);
  CHECK(len == dest.size());
}

#if OPENSSL_VERSION_NUMBER < 0x10100000L
namespace {
std::vector<RwMutex> &openssl_mutexes() {
  static std::vector<RwMutex> mutexes(CRYPTO_num_locks());
  return mutexes;
}

void openssl_threadid_callback(CRYPTO_THREADID *thread_id) {
  static TD_THREAD_LOCAL int id;
  CRYPTO_THREADID_set_pointer(thread_id, &id);
}

void openssl_locking_function(int mode, int n, const char *file, int line) {
  auto &mutexes = openssl_mutexes();
  if (mode & CRYPTO_LOCK) {
    if (mode & CRYPTO_READ) {
      mutexes[n].lock_read_unsafe();
    } else {
      mutexes[n].lock_write_unsafe();
    }
  } else {
    if (mode & CRYPTO_READ) {
      mutexes[n].unlock_read_unsafe();
    } else {
      mutexes[n].unlock_write_unsafe();
    }
  }
}
}  // namespace
#endif

void init_openssl_threads() {
#if OPENSSL_VERSION_NUMBER < 0x10100000L
  if (CRYPTO_get_locking_callback() == nullptr) {
    CRYPTO_THREADID_set_callback(openssl_threadid_callback);
    CRYPTO_set_locking_callback(openssl_locking_function);
  }
#endif
}

}  // end namespace td
