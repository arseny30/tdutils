#pragma once

#include "td/utils/common.h"
#include "td/utils/Slice.h"

namespace td {

uint64 pq_factorize(uint64 pq);

#if TD_HAVE_OPENSSL
void init_crypto();

int pq_factorize(Slice pq_str, string *p_str, string *q_str);

void aes_ige_encrypt(const UInt256 &aes_key, UInt256 *aes_iv, Slice from, MutableSlice to);
void aes_ige_decrypt(const UInt256 &aes_key, UInt256 *aes_iv, Slice from, MutableSlice to);

void aes_cbc_encrypt(const UInt256 &aes_key, UInt128 *aes_iv, Slice from, MutableSlice to);
void aes_cbc_decrypt(const UInt256 &aes_key, UInt128 *aes_iv, Slice from, MutableSlice to);

class AesCtrState {
 public:
  AesCtrState();
  AesCtrState(const AesCtrState &from) = delete;
  AesCtrState &operator=(const AesCtrState &from) = delete;
  AesCtrState(AesCtrState &&from);
  AesCtrState &operator=(AesCtrState &&from);
  ~AesCtrState();

  void init(const UInt256 &key, const UInt128 &iv);

  void encrypt(Slice from, MutableSlice to);

  void decrypt(Slice from, MutableSlice to);

 private:
  struct Impl;
  std::unique_ptr<Impl> ctx_;
};

void sha1(Slice data, unsigned char output[20]);

void sha256(Slice data, MutableSlice output);

struct Sha256StateImpl;

struct Sha256State {
  Sha256State();
  Sha256State(Sha256State &&from);
  Sha256State &operator=(Sha256State &&from);
  ~Sha256State();
  std::unique_ptr<Sha256StateImpl> impl;
};

void sha256_init(Sha256State *state);
void sha256_update(Slice data, Sha256State *state);
void sha256_final(Sha256State *state, MutableSlice output);

void md5(Slice input, MutableSlice output);

void pbkdf2_sha256(Slice password, Slice salt, int iteration_count, MutableSlice dest);
void hmac_sha256(Slice key, Slice message, MutableSlice dest);

void init_openssl_threads();
#endif

#if TD_HAVE_ZLIB
uint32 crc32(Slice data);
#endif

uint64 crc64(Slice data);

}  // namespace td
