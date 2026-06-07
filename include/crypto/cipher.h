#ifndef CIFRASYNC_CRYPTO_CIPHER_H
#define CIFRASYNC_CRYPTO_CIPHER_H

#include <stddef.h>

#include "common/types.h"
#include "crypto/kdf.h"

#define CS_CRYPTO_BLOB_MAGIC_SIZE 8U
#define CS_CRYPTO_BLOB_VERSION 2U
#define CS_CRYPTO_TAG_SIZE CS_CRYPTO_SHA256_SIZE

typedef struct cs_cipher_blob_header {
	unsigned char magic[CS_CRYPTO_BLOB_MAGIC_SIZE];
	cs_u32 version;
	cs_u32 rounds;
	cs_u64 plaintext_size;
	unsigned char salt[CS_CRYPTO_SALT_SIZE];
	unsigned char nonce[CS_CRYPTO_NONCE_SIZE];
} cs_cipher_blob_header_t;

int cs_cipher_seal_alloc(const char *passphrase,
	const void *plaintext,
	size_t plaintext_size,
	unsigned char **out_blob,
	size_t *out_blob_size);

int cs_cipher_open_alloc(const char *passphrase,
	const void *blob,
	size_t blob_size,
	unsigned char **out_plaintext,
	size_t *out_plaintext_size);

int cs_cipher_seal_buffer(const char *passphrase,
	const void *plaintext,
	size_t plaintext_size,
	void *blob,
	size_t blob_capacity,
	size_t *out_blob_size);

int cs_cipher_open_buffer(const char *passphrase,
	const void *blob,
	size_t blob_size,
	void *plaintext,
	size_t plaintext_capacity,
	size_t *out_plaintext_size);

size_t cs_cipher_blob_overhead(void);

int cs_cipher_cache_init(void);
void cs_cipher_cache_clear(void);
void cs_cipher_cache_free(void);

#endif

