#include "crypto/cipher.h"

#include "common/memory.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static const unsigned char cs_cipher_magic[CS_CRYPTO_BLOB_MAGIC_SIZE] = {'C', 'S', 'E', 'N', 'C', '0', '0', '1'};

static void cs_write_u64_le(unsigned char *out, cs_u64 value) {
	for (size_t index = 0; index < 8U; ++index) {
		out[index] = (unsigned char)((value >> (8U * index)) & 0xffU);
	}
}

static int cs_constant_time_equal(const unsigned char *lhs, const unsigned char *rhs, size_t size) {
	unsigned char diff = 0U;
	for (size_t index = 0; index < size; ++index) {
		diff |= (unsigned char)(lhs[index] ^ rhs[index]);
	}
	return diff == 0U;
}

static void cs_cipher_stream_block(const unsigned char key[CS_CRYPTO_SHA256_SIZE], const unsigned char nonce[CS_CRYPTO_NONCE_SIZE], cs_u64 counter, unsigned char out_block[CS_CRYPTO_SHA256_SIZE]) {
	unsigned char counter_block[8];
	cs_hmac_sha256_ctx_t ctx;
	cs_write_u64_le(counter_block, counter);
	cs_hmac_sha256_init(&ctx, key, CS_CRYPTO_SHA256_SIZE);
	cs_hmac_sha256_update(&ctx, nonce, CS_CRYPTO_NONCE_SIZE);
	cs_hmac_sha256_update(&ctx, counter_block, sizeof(counter_block));
	cs_hmac_sha256_final(&ctx, out_block);
	cs_memzero(counter_block, sizeof(counter_block));
	cs_memzero(&ctx, sizeof(ctx));
}

static size_t cs_cipher_header_size(void) {
	return sizeof(cs_cipher_blob_header_t);
}

size_t cs_cipher_blob_overhead(void) {
	return cs_cipher_header_size() + CS_CRYPTO_TAG_SIZE;
}

static int cs_cipher_pack_header(cs_cipher_blob_header_t *header, size_t plaintext_size, unsigned int rounds, const unsigned char salt[CS_CRYPTO_SALT_SIZE], const unsigned char nonce[CS_CRYPTO_NONCE_SIZE]) {
	if (header == NULL) {
		return -1;
	}
	memcpy(header->magic, cs_cipher_magic, sizeof(cs_cipher_magic));
	header->version = CS_CRYPTO_BLOB_VERSION;
	header->rounds = (cs_u32)rounds;
	header->plaintext_size = (cs_u64)plaintext_size;
	memcpy(header->salt, salt, CS_CRYPTO_SALT_SIZE);
	memcpy(header->nonce, nonce, CS_CRYPTO_NONCE_SIZE);
	return 0;
}

static int cs_cipher_parse_header(const void *blob, size_t blob_size, cs_cipher_blob_header_t *header_out, const unsigned char **ciphertext_out, const unsigned char **tag_out) {
	const unsigned char *bytes = (const unsigned char *)blob;
	if (blob == NULL || blob_size < cs_cipher_header_size() + CS_CRYPTO_TAG_SIZE || header_out == NULL || ciphertext_out == NULL || tag_out == NULL) {
		return -1;
	}

	memcpy(header_out, bytes, sizeof(*header_out));
	if (memcmp(header_out->magic, cs_cipher_magic, sizeof(cs_cipher_magic)) != 0) {
		return -1;
	}
	if (header_out->version != CS_CRYPTO_BLOB_VERSION) {
		return -1;
	}

	const size_t expected_size = cs_cipher_header_size() + (size_t)header_out->plaintext_size + CS_CRYPTO_TAG_SIZE;
	if (expected_size != blob_size) {
		return -1;
	}

	*ciphertext_out = bytes + cs_cipher_header_size();
	*tag_out = bytes + cs_cipher_header_size() + (size_t)header_out->plaintext_size;
	return 0;
}

static int cs_cipher_derive_key(const char *passphrase, const cs_cipher_blob_header_t *header, unsigned char out_key[CS_CRYPTO_SHA256_SIZE]) {
	return cs_kdf_derive_key(passphrase, header->salt, CS_CRYPTO_SALT_SIZE, header->rounds, out_key, CS_CRYPTO_SHA256_SIZE);
}

static int cs_cipher_seal_internal(const char *passphrase,
	const void *plaintext,
	size_t plaintext_size,
	unsigned char *blob,
	size_t blob_capacity,
	size_t *out_blob_size) {
	cs_cipher_blob_header_t header;
	unsigned char salt[CS_CRYPTO_SALT_SIZE];
	unsigned char nonce[CS_CRYPTO_NONCE_SIZE];
	unsigned char key[CS_CRYPTO_SHA256_SIZE];
	unsigned char tag[CS_CRYPTO_TAG_SIZE];
	cs_hmac_sha256_ctx_t tag_ctx;
	const unsigned char *plain_bytes = (const unsigned char *)plaintext;
	unsigned char *cipher_bytes;
	size_t required_size;

	if (passphrase == NULL || (plain_bytes == NULL && plaintext_size > 0U) || blob == NULL || out_blob_size == NULL) {
		return -1;
	}
	if (plaintext_size > (size_t)SIZE_MAX - cs_cipher_blob_overhead()) {
		return -1;
	}

	required_size = cs_cipher_blob_overhead() + plaintext_size;
	if (blob_capacity < required_size) {
		return -1;
	}

	if (cs_kdf_generate_salt(salt, sizeof(salt)) != 0) {
		return -1;
	}
	if (cs_kdf_generate_salt(nonce, sizeof(nonce)) != 0) {
		cs_memzero(salt, sizeof(salt));
		return -1;
	}

	if (cs_cipher_pack_header(&header, plaintext_size, CS_CRYPTO_DEFAULT_PBKDF2_ROUNDS, salt, nonce) != 0) {
		cs_memzero(salt, sizeof(salt));
		cs_memzero(nonce, sizeof(nonce));
		return -1;
	}
	if (cs_cipher_derive_key(passphrase, &header, key) != 0) {
		cs_memzero(salt, sizeof(salt));
		cs_memzero(nonce, sizeof(nonce));
		cs_memzero(&header, sizeof(header));
		return -1;
	}

	memcpy(blob, &header, sizeof(header));
	cipher_bytes = blob + sizeof(header);
	cs_hmac_sha256_init(&tag_ctx, key, sizeof(key));
	cs_hmac_sha256_update(&tag_ctx, &header, sizeof(header));

	for (size_t offset = 0U; offset < plaintext_size; offset += CS_CRYPTO_SHA256_SIZE) {
		unsigned char stream[CS_CRYPTO_SHA256_SIZE];
		const size_t chunk_size = (plaintext_size - offset) < CS_CRYPTO_SHA256_SIZE ? (plaintext_size - offset) : CS_CRYPTO_SHA256_SIZE;
		const cs_u64 block_index = (cs_u64)(offset / CS_CRYPTO_SHA256_SIZE);
		cs_cipher_stream_block(key, nonce, block_index, stream);
		for (size_t index = 0U; index < chunk_size; ++index) {
			cipher_bytes[offset + index] = (unsigned char)(plain_bytes[offset + index] ^ stream[index]);
		}
		cs_hmac_sha256_update(&tag_ctx, cipher_bytes + offset, chunk_size);
		cs_memzero(stream, sizeof(stream));
	}

	cs_hmac_sha256_final(&tag_ctx, tag);
	memcpy(blob + sizeof(header) + plaintext_size, tag, sizeof(tag));
	*out_blob_size = required_size;

	cs_memzero(salt, sizeof(salt));
	cs_memzero(nonce, sizeof(nonce));
	cs_memzero(key, sizeof(key));
	cs_memzero(tag, sizeof(tag));
	cs_memzero(&tag_ctx, sizeof(tag_ctx));
	cs_memzero(&header, sizeof(header));
	return 0;
}

static int cs_cipher_open_internal(const char *passphrase,
	const void *blob,
	size_t blob_size,
	void *plaintext,
	size_t plaintext_capacity,
	size_t *out_plaintext_size) {
	cs_cipher_blob_header_t header;
	const unsigned char *cipher_bytes;
	const unsigned char *tag_bytes;
	unsigned char key[CS_CRYPTO_SHA256_SIZE];
	unsigned char expected_tag[CS_CRYPTO_TAG_SIZE];
	cs_hmac_sha256_ctx_t tag_ctx;
	unsigned char *plain_bytes = (unsigned char *)plaintext;

	if (passphrase == NULL || out_plaintext_size == NULL || (plaintext == NULL && plaintext_capacity > 0U)) {
		return -1;
	}
	if (cs_cipher_parse_header(blob, blob_size, &header, &cipher_bytes, &tag_bytes) != 0) {
		return -1;
	}
	if ((size_t)header.plaintext_size > plaintext_capacity) {
		return -1;
	}
	if (cs_cipher_derive_key(passphrase, &header, key) != 0) {
		cs_memzero(&header, sizeof(header));
		return -1;
	}

	cs_hmac_sha256_init(&tag_ctx, key, sizeof(key));
	cs_hmac_sha256_update(&tag_ctx, &header, sizeof(header));
	cs_hmac_sha256_update(&tag_ctx, cipher_bytes, (size_t)header.plaintext_size);
	cs_hmac_sha256_final(&tag_ctx, expected_tag);

	if (!cs_constant_time_equal(expected_tag, tag_bytes, sizeof(expected_tag))) {
		cs_memzero(key, sizeof(key));
		cs_memzero(expected_tag, sizeof(expected_tag));
		cs_memzero(&tag_ctx, sizeof(tag_ctx));
		cs_memzero(&header, sizeof(header));
		return -1;
	}

	for (size_t offset = 0U; offset < (size_t)header.plaintext_size; offset += CS_CRYPTO_SHA256_SIZE) {
		unsigned char stream[CS_CRYPTO_SHA256_SIZE];
		const size_t chunk_size = ((size_t)header.plaintext_size - offset) < CS_CRYPTO_SHA256_SIZE ? ((size_t)header.plaintext_size - offset) : CS_CRYPTO_SHA256_SIZE;
		const cs_u64 block_index = (cs_u64)(offset / CS_CRYPTO_SHA256_SIZE);
		cs_cipher_stream_block(key, header.nonce, block_index, stream);
		for (size_t index = 0U; index < chunk_size; ++index) {
			plain_bytes[offset + index] = (unsigned char)(cipher_bytes[offset + index] ^ stream[index]);
		}
		cs_memzero(stream, sizeof(stream));
	}

	*out_plaintext_size = (size_t)header.plaintext_size;
	cs_memzero(key, sizeof(key));
	cs_memzero(expected_tag, sizeof(expected_tag));
	cs_memzero(&tag_ctx, sizeof(tag_ctx));
	cs_memzero(&header, sizeof(header));
	return 0;
}

int cs_cipher_seal_buffer(const char *passphrase,
	const void *plaintext,
	size_t plaintext_size,
	void *blob,
	size_t blob_capacity,
	size_t *out_blob_size) {
	return cs_cipher_seal_internal(passphrase, plaintext, plaintext_size, (unsigned char *)blob, blob_capacity, out_blob_size);
}

int cs_cipher_open_buffer(const char *passphrase,
	const void *blob,
	size_t blob_size,
	void *plaintext,
	size_t plaintext_capacity,
	size_t *out_plaintext_size) {
	return cs_cipher_open_internal(passphrase, blob, blob_size, plaintext, plaintext_capacity, out_plaintext_size);
}

int cs_cipher_seal_alloc(const char *passphrase,
	const void *plaintext,
	size_t plaintext_size,
	unsigned char **out_blob,
	size_t *out_blob_size) {
	unsigned char *blob;
	size_t blob_size;
	int status;

	if (out_blob == NULL || out_blob_size == NULL) {
		return -1;
	}

	blob = (unsigned char *)malloc(cs_cipher_blob_overhead() + plaintext_size);
	if (blob == NULL) {
		return -1;
	}

	status = cs_cipher_seal_internal(passphrase, plaintext, plaintext_size, blob, cs_cipher_blob_overhead() + plaintext_size, &blob_size);
	if (status != 0) {
		cs_memzero(blob, cs_cipher_blob_overhead() + plaintext_size);
		free(blob);
		return -1;
	}

	*out_blob = blob;
	*out_blob_size = blob_size;
	return 0;
}

int cs_cipher_open_alloc(const char *passphrase,
	const void *blob,
	size_t blob_size,
	unsigned char **out_plaintext,
	size_t *out_plaintext_size) {
	unsigned char *plaintext;
	cs_cipher_blob_header_t header;
	const unsigned char *cipher_bytes;
	const unsigned char *tag_bytes;
	int status;

	if (out_plaintext == NULL || out_plaintext_size == NULL) {
		return -1;
	}
	if (cs_cipher_parse_header(blob, blob_size, &header, &cipher_bytes, &tag_bytes) != 0) {
		return -1;
	}

	plaintext = (unsigned char *)malloc((size_t)header.plaintext_size);
	if (plaintext == NULL && header.plaintext_size > 0U) {
		return -1;
	}

	status = cs_cipher_open_internal(passphrase, blob, blob_size, plaintext, (size_t)header.plaintext_size, out_plaintext_size);
	if (status != 0) {
		if (plaintext != NULL) {
			cs_memzero(plaintext, (size_t)header.plaintext_size);
			free(plaintext);
		}
		return -1;
	}

	*out_plaintext = plaintext;
	return 0;
}

