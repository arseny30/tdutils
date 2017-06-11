#pragma once

#include "td/utils/common.h"
#include "td/utils/Slice.h"

namespace td {

uint64 pq_factorize(uint64 pq);

int pq_factorize(Slice pq_str, string *p_str, string *q_str);

/*** AES ***/
ssize_t aes_ige_xcrypt(const UInt256 &aes_key, UInt256 *aes_iv, Slice from, MutableSlice to, bool encrypt_flag);
ssize_t aes_ige_encrypt(const UInt256 &aes_key, UInt256 *aes_iv, Slice from, MutableSlice to);
ssize_t aes_ige_decrypt(const UInt256 &aes_key, UInt256 *aes_iv, Slice from, MutableSlice to);

struct AesCtrStateImpl;
struct AesCtrState {
  AesCtrState();
  AesCtrState(AesCtrState &&from);
  AesCtrState &operator=(AesCtrState &&from);
  ~AesCtrState();

  std::unique_ptr<AesCtrStateImpl> ctx_;
};
void init_aes_ctr_state(const UInt256 &key, const UInt128 &iv, AesCtrState *state);
ssize_t aes_ctr_xcrypt(AesCtrState *state, Slice from, MutableSlice to, bool encrypt_flag);
ssize_t aes_ctr_encrypt(AesCtrState *state, Slice from, MutableSlice to);
ssize_t aes_ctr_decrypt(AesCtrState *state, Slice from, MutableSlice to);

// cbc
ssize_t aes_cbc_xcrypt(const UInt256 &aes_key, UInt128 *aes_iv, Slice from, MutableSlice to, bool encrypt_flag);
ssize_t aes_cbc_encrypt(const UInt256 &aes_key, UInt128 *aes_iv, Slice from, MutableSlice to);
ssize_t aes_cbc_decrypt(const UInt256 &aes_key, UInt128 *aes_iv, Slice from, MutableSlice to);

/*** SHA-1 ***/
void sha1(Slice data, unsigned char output[20]);

uint32 crc32(Slice data);
uint64 crc64(Slice data);

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

}  // namespace td
