#ifndef CIFRASYNC_CRYPTO_KDF_H
#define CIFRASYNC_CRYPTO_KDF_H

#include <stddef.h>

#include "common/types.h"

#define CS_CRYPTO_SHA256_SIZE 32U
#define CS_CRYPTO_SHA256_BLOCK_SIZE 64U
#define CS_CRYPTO_SALT_SIZE 16U
#define CS_CRYPTO_NONCE_SIZE 16U
#define CS_CRYPTO_DEFAULT_PBKDF2_ROUNDS 600000U

typedef struct cs_sha256_ctx {
	cs_u32 state[8];
	cs_u64 bit_len;
	unsigned char buffer[64];
	size_t buffer_len;
} cs_sha256_ctx_t;

typedef struct cs_hmac_sha256_ctx {
	cs_sha256_ctx_t inner;
	cs_sha256_ctx_t outer;
} cs_hmac_sha256_ctx_t;

void cs_sha256_init(cs_sha256_ctx_t *ctx);
void cs_sha256_update(cs_sha256_ctx_t *ctx, const void *data, size_t size);
void cs_sha256_final(cs_sha256_ctx_t *ctx, unsigned char out_hash[CS_CRYPTO_SHA256_SIZE]);

void cs_hmac_sha256_init(cs_hmac_sha256_ctx_t *ctx, const void *key, size_t key_size);
void cs_hmac_sha256_update(cs_hmac_sha256_ctx_t *ctx, const void *data, size_t size);
void cs_hmac_sha256_final(cs_hmac_sha256_ctx_t *ctx, unsigned char out_mac[CS_CRYPTO_SHA256_SIZE]);

int cs_kdf_generate_salt(unsigned char out_salt[CS_CRYPTO_SALT_SIZE], size_t salt_size);
int cs_kdf_generate_random(void *out, size_t out_size);
int cs_kdf_sha256(const void *data, size_t data_size, unsigned char out_hash[CS_CRYPTO_SHA256_SIZE]);
int cs_kdf_hmac_sha256(const void *key, size_t key_size, const void *data, size_t data_size, unsigned char out_mac[CS_CRYPTO_SHA256_SIZE]);
int cs_kdf_pbkdf2_sha256(const char *passphrase,
	const unsigned char *salt,
	size_t salt_size,
	unsigned int rounds,
	unsigned char *out_key,
	size_t out_key_size);
int cs_kdf_derive_key(const char *passphrase,
	const unsigned char *salt,
	size_t salt_size,
	unsigned int rounds,
	unsigned char *out_key,
	size_t out_key_size);

#endif

