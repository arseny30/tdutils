#pragma once

#include "td/utils/buffer.h"
#include "td/utils/common.h"
#include "td/utils/SharedSlice.h"
#include "td/utils/Slice.h"
#include "td/utils/Status.h"

namespace td {

uint64 pq_factorize(uint64 pq);

#if TD_HAVE_OPENSSL
void init_crypto();

int pq_factorize(Slice pq_str, string *p_str, string *q_str);
class Evp {
 public:
  Evp() = default;
  Evp(const Evp &from) = delete;
  Evp &operator=(const Evp &from) = delete;
  Evp(Evp &&from);
  Evp &operator=(Evp &&from);
  ~Evp();

  void init_encrypt_ecb(Slice key);
  void init_decrypt_ecb(Slice key);
  void init_encrypt_cbc(Slice key);
  void init_decrypt_cbc(Slice key);
  void init_iv(Slice iv);
  void encrypt(const uint8 *src, uint8 *dst, int size);
  void decrypt(const uint8 *src, uint8 *dst, int size);

  struct EvpDeleter {
   public:
    void operator()(void *ptr);
  };
  enum class Type : int8 { Empty, Ecb, Cbc };

 private:
  std::unique_ptr<void, EvpDeleter> ctx_;
};

struct AesBlock {
  uint64 hi;
  uint64 lo;

  uint8 *raw();
  const uint8 *raw() const;
  Slice as_slice() const;

  AesBlock operator^(const AesBlock &b) const;
  void operator^=(const AesBlock &b);

  void load(const uint8 *from);
  void store(uint8 *to);

  AesBlock inc() const;
};

struct AesCtrCounterPack {
  static constexpr size_t BLOCK_COUNT = 32;
  AesBlock blocks[BLOCK_COUNT];
  uint8 *raw();
  const uint8 *raw() const;

  size_t size() const;

  Slice as_slice() const;
  MutableSlice as_mutable_slice();

  void init(AesBlock block);
  void rotate();
};

class AesState {
 public:
  AesState();
  AesState(const AesState &from) = delete;
  AesState &operator=(const AesState &from) = delete;
  AesState(AesState &&from);
  AesState &operator=(AesState &&from);
  ~AesState();

  void init(Slice key, bool encrypt);

  void encrypt(const uint8 *src, uint8 *dst, int size);

  void decrypt(const uint8 *src, uint8 *dst, int size);

 private:
  struct Impl;
  unique_ptr<Impl> impl_;
};

void aes_ige_encrypt(Slice aes_key, MutableSlice aes_iv, Slice from, MutableSlice to);
void aes_ige_decrypt(Slice aes_key, MutableSlice aes_iv, Slice from, MutableSlice to);

class AesIgeState {
 public:
  AesIgeState();
  AesIgeState(const AesIgeState &from) = delete;
  AesIgeState &operator=(const AesIgeState &from) = delete;
  AesIgeState(AesIgeState &&from);
  AesIgeState &operator=(AesIgeState &&from);
  ~AesIgeState();

  void init(Slice key, Slice iv, bool encrypt);

  void encrypt(Slice from, MutableSlice to);

  void decrypt(Slice from, MutableSlice to);

 private:
  AesBlock encrypted_iv_;
  AesBlock plaintext_iv_;
  Evp evp_;
};

void aes_cbc_encrypt(Slice aes_key, MutableSlice aes_iv, Slice from, MutableSlice to);
void aes_cbc_decrypt(Slice aes_key, MutableSlice aes_iv, Slice from, MutableSlice to);

class AesCtrState {
 public:
  AesCtrState() = default;
  AesCtrState(const AesCtrState &from) = delete;
  AesCtrState &operator=(const AesCtrState &from) = delete;
  AesCtrState(AesCtrState &&from) = default;
  AesCtrState &operator=(AesCtrState &&from) = default;
  ~AesCtrState() = default;

  void init(Slice key, Slice iv);

  void encrypt(Slice from, MutableSlice to);

  void decrypt(Slice from, MutableSlice to);

 private:
  Evp evp_;

  AesCtrCounterPack counter_;
  AesCtrCounterPack encrypted_counter_;
  Slice current_;

  void fill();
};

class AesCbcState {
 public:
  AesCbcState(Slice key256, Slice iv128);

  void encrypt(Slice from, MutableSlice to);
  void decrypt(Slice from, MutableSlice to);

 private:
  SecureString key_;
  SecureString iv_;
};

void sha1(Slice data, unsigned char output[20]);

void sha256(Slice data, MutableSlice output);

void sha512(Slice data, MutableSlice output);

string sha256(Slice data) TD_WARN_UNUSED_RESULT;

string sha512(Slice data) TD_WARN_UNUSED_RESULT;

class Sha256State {
 public:
  Sha256State();
  Sha256State(const Sha256State &other) = delete;
  Sha256State &operator=(const Sha256State &other) = delete;
  Sha256State(Sha256State &&other);
  Sha256State &operator=(Sha256State &&other);
  ~Sha256State();

  void init();

  void feed(Slice data);

  void extract(MutableSlice dest, bool destroy = false);

 private:
  class Impl;
  unique_ptr<Impl> impl_;
  bool is_inited_ = false;
};

void md5(Slice input, MutableSlice output);

void pbkdf2_sha256(Slice password, Slice salt, int iteration_count, MutableSlice dest);
void pbkdf2_sha512(Slice password, Slice salt, int iteration_count, MutableSlice dest);

void hmac_sha256(Slice key, Slice message, MutableSlice dest);
void hmac_sha512(Slice key, Slice message, MutableSlice dest);

// Interface may be improved
Result<BufferSlice> rsa_encrypt_pkcs1_oaep(Slice public_key, Slice data);
Result<BufferSlice> rsa_decrypt_pkcs1_oaep(Slice private_key, Slice data);

void init_openssl_threads();

Status create_openssl_error(int code, Slice message);

void clear_openssl_errors(Slice source);
#endif

#if TD_HAVE_ZLIB
uint32 crc32(Slice data);
#endif

#if TD_HAVE_CRC32C
uint32 crc32c(Slice data);
uint32 crc32c_extend(uint32 old_crc, Slice data);
uint32 crc32c_extend(uint32 old_crc, uint32 new_crc, size_t data_size);
#endif

uint64 crc64(Slice data);
uint16 crc16(Slice data);

}  // namespace td
