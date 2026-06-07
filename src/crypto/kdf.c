#include "crypto/kdf.h"

#include "common/memory.h"

#include <string.h>
#include <time.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
#pragma comment(lib, "advapi32")
#else
#include <unistd.h>
#include <fcntl.h>
#endif

static const cs_u32 cs_sha256_k[64] = {
	0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U,
	0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
	0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
	0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
	0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
	0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
	0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
	0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
	0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
	0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
	0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U,
	0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
	0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U,
	0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
	0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
	0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U
};

static cs_u32 cs_rotr32(cs_u32 value, cs_u32 shift) {
	return (value >> shift) | (value << (32U - shift));
}

static cs_u32 cs_load_be32(const unsigned char *p) {
	return ((cs_u32)p[0] << 24) | ((cs_u32)p[1] << 16) | ((cs_u32)p[2] << 8) | (cs_u32)p[3];
}

static void cs_store_be32(unsigned char *p, cs_u32 value) {
	p[0] = (unsigned char)(value >> 24);
	p[1] = (unsigned char)(value >> 16);
	p[2] = (unsigned char)(value >> 8);
	p[3] = (unsigned char)value;
}

static void cs_store_be64(unsigned char *p, cs_u64 value) {
	for (size_t index = 0; index < 8U; ++index) {
		p[7U - index] = (unsigned char)(value >> (index * 8U));
	}
}

static void cs_sha256_transform(cs_sha256_ctx_t *ctx, const unsigned char block[64]) {
	cs_u32 w[64];
	for (size_t index = 0; index < 16U; ++index) {
		w[index] = cs_load_be32(block + (index * 4U));
	}
	for (size_t index = 16U; index < 64U; ++index) {
		cs_u32 s0 = cs_rotr32(w[index - 15U], 7U) ^ cs_rotr32(w[index - 15U], 18U) ^ (w[index - 15U] >> 3U);
		cs_u32 s1 = cs_rotr32(w[index - 2U], 17U) ^ cs_rotr32(w[index - 2U], 19U) ^ (w[index - 2U] >> 10U);
		w[index] = w[index - 16U] + s0 + w[index - 7U] + s1;
	}

	cs_u32 a = ctx->state[0];
	cs_u32 b = ctx->state[1];
	cs_u32 c = ctx->state[2];
	cs_u32 d = ctx->state[3];
	cs_u32 e = ctx->state[4];
	cs_u32 f = ctx->state[5];
	cs_u32 g = ctx->state[6];
	cs_u32 h = ctx->state[7];

	for (size_t index = 0; index < 64U; ++index) {
		cs_u32 s1 = cs_rotr32(e, 6U) ^ cs_rotr32(e, 11U) ^ cs_rotr32(e, 25U);
		cs_u32 ch = (e & f) ^ ((~e) & g);
		cs_u32 temp1 = h + s1 + ch + cs_sha256_k[index] + w[index];
		cs_u32 s0 = cs_rotr32(a, 2U) ^ cs_rotr32(a, 13U) ^ cs_rotr32(a, 22U);
		cs_u32 maj = (a & b) ^ (a & c) ^ (b & c);
		cs_u32 temp2 = s0 + maj;

		h = g;
		g = f;
		f = e;
		e = d + temp1;
		d = c;
		c = b;
		b = a;
		a = temp1 + temp2;
	}

	ctx->state[0] += a;
	ctx->state[1] += b;
	ctx->state[2] += c;
	ctx->state[3] += d;
	ctx->state[4] += e;
	ctx->state[5] += f;
	ctx->state[6] += g;
	ctx->state[7] += h;
	cs_memzero(w, sizeof(w));
}

void cs_sha256_init(cs_sha256_ctx_t *ctx) {
	ctx->state[0] = 0x6a09e667U;
	ctx->state[1] = 0xbb67ae85U;
	ctx->state[2] = 0x3c6ef372U;
	ctx->state[3] = 0xa54ff53aU;
	ctx->state[4] = 0x510e527fU;
	ctx->state[5] = 0x9b05688cU;
	ctx->state[6] = 0x1f83d9abU;
	ctx->state[7] = 0x5be0cd19U;
	ctx->bit_len = 0U;
	ctx->buffer_len = 0U;
	cs_memzero(ctx->buffer, sizeof(ctx->buffer));
}

void cs_sha256_update(cs_sha256_ctx_t *ctx, const void *data, size_t size) {
	const unsigned char *input = (const unsigned char *)data;
	if (size == 0U) {
		return;
	}

	ctx->bit_len += (cs_u64)size * 8U;
	while (size > 0U) {
		size_t space = 64U - ctx->buffer_len;
		size_t copy = size < space ? size : space;
		memcpy(ctx->buffer + ctx->buffer_len, input, copy);
		ctx->buffer_len += copy;
		input += copy;
		size -= copy;
		if (ctx->buffer_len == 64U) {
			cs_sha256_transform(ctx, ctx->buffer);
			ctx->buffer_len = 0U;
		}
	}
}

void cs_sha256_final(cs_sha256_ctx_t *ctx, unsigned char out_hash[CS_CRYPTO_SHA256_SIZE]) {
	ctx->buffer[ctx->buffer_len++] = 0x80U;
	if (ctx->buffer_len > 56U) {
		while (ctx->buffer_len < 64U) {
			ctx->buffer[ctx->buffer_len++] = 0U;
		}
		cs_sha256_transform(ctx, ctx->buffer);
		ctx->buffer_len = 0U;
	}
	while (ctx->buffer_len < 56U) {
		ctx->buffer[ctx->buffer_len++] = 0U;
	}
	cs_store_be64(ctx->buffer + 56U, ctx->bit_len);
	cs_sha256_transform(ctx, ctx->buffer);

	for (size_t index = 0; index < 8U; ++index) {
		cs_store_be32(out_hash + (index * 4U), ctx->state[index]);
	}
	cs_memzero(ctx, sizeof(*ctx));
}

void cs_hmac_sha256_init(cs_hmac_sha256_ctx_t *ctx, const void *key, size_t key_size) {
	unsigned char key_block[CS_CRYPTO_SHA256_BLOCK_SIZE];
	unsigned char key_hash[CS_CRYPTO_SHA256_SIZE];
	const unsigned char *key_bytes = (const unsigned char *)key;

	cs_memzero(key_block, sizeof(key_block));
	if (key_size > CS_CRYPTO_SHA256_BLOCK_SIZE) {
		cs_sha256_ctx_t key_ctx;
		cs_sha256_init(&key_ctx);
		cs_sha256_update(&key_ctx, key_bytes, key_size);
		cs_sha256_final(&key_ctx, key_hash);
		memcpy(key_block, key_hash, CS_CRYPTO_SHA256_SIZE);
		cs_memzero(key_hash, sizeof(key_hash));
	} else if (key_size > 0U) {
		memcpy(key_block, key_bytes, key_size);
	}

	for (size_t index = 0; index < CS_CRYPTO_SHA256_BLOCK_SIZE; ++index) {
		key_block[index] ^= 0x36U;
	}
	cs_sha256_init(&ctx->inner);
	cs_sha256_update(&ctx->inner, key_block, sizeof(key_block));

	for (size_t index = 0; index < CS_CRYPTO_SHA256_BLOCK_SIZE; ++index) {
		key_block[index] ^= 0x36U ^ 0x5cU;
	}
	cs_sha256_init(&ctx->outer);
	cs_sha256_update(&ctx->outer, key_block, sizeof(key_block));

	cs_memzero(key_block, sizeof(key_block));
}

void cs_hmac_sha256_update(cs_hmac_sha256_ctx_t *ctx, const void *data, size_t size) {
	cs_sha256_update(&ctx->inner, data, size);
}

void cs_hmac_sha256_final(cs_hmac_sha256_ctx_t *ctx, unsigned char out_mac[CS_CRYPTO_SHA256_SIZE]) {
	unsigned char inner_hash[CS_CRYPTO_SHA256_SIZE];
	cs_sha256_final(&ctx->inner, inner_hash);
	cs_sha256_update(&ctx->outer, inner_hash, sizeof(inner_hash));
	cs_sha256_final(&ctx->outer, out_mac);
	cs_memzero(inner_hash, sizeof(inner_hash));
}

int cs_kdf_sha256(const void *data, size_t data_size, unsigned char out_hash[CS_CRYPTO_SHA256_SIZE]) {
	if (out_hash == NULL || (data == NULL && data_size > 0U)) {
		return -1;
	}

	cs_sha256_ctx_t ctx;
	cs_sha256_init(&ctx);
	cs_sha256_update(&ctx, data, data_size);
	cs_sha256_final(&ctx, out_hash);
	return 0;
}

int cs_kdf_hmac_sha256(const void *key, size_t key_size, const void *data, size_t data_size, unsigned char out_mac[CS_CRYPTO_SHA256_SIZE]) {
	if (out_mac == NULL || (key == NULL && key_size > 0U) || (data == NULL && data_size > 0U)) {
		return -1;
	}

	cs_hmac_sha256_ctx_t ctx;
	cs_hmac_sha256_init(&ctx, key, key_size);
	cs_hmac_sha256_update(&ctx, data, data_size);
	cs_hmac_sha256_final(&ctx, out_mac);
	return 0;
}

int cs_kdf_generate_random(void *out, size_t out_size) {
	if (out == NULL || out_size == 0U) {
		return -1;
	}
#ifdef _WIN32
	{
		HCRYPTPROV provider = 0;
		if (CryptAcquireContextA(&provider, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT) == 0) {
			return -1;
		}
		if (CryptGenRandom(provider, (DWORD)out_size, (BYTE *)out) == 0) {
			CryptReleaseContext(provider, 0);
			return -1;
		}
		CryptReleaseContext(provider, 0);
	}
#else
	{
		int fd = open("/dev/urandom", O_RDONLY);
		if (fd < 0) {
			return -1;
		}
		size_t total = 0U;
		while (total < out_size) {
			ssize_t n = read(fd, (unsigned char *)out + total, out_size - total);
			if (n <= 0) {
				close(fd);
				return -1;
			}
			total += (size_t)n;
		}
		close(fd);
	}
#endif
	return 0;
}

int cs_kdf_generate_salt(unsigned char out_salt[CS_CRYPTO_SALT_SIZE], size_t salt_size) {
	if (out_salt == NULL || salt_size != CS_CRYPTO_SALT_SIZE) {
		return -1;
	}
	return cs_kdf_generate_random(out_salt, salt_size);
}

int cs_kdf_pbkdf2_sha256(const char *passphrase,
	const unsigned char *salt,
	size_t salt_size,
	unsigned int rounds,
	unsigned char *out_key,
	size_t out_key_size) {
	if (passphrase == NULL || salt == NULL || out_key == NULL || salt_size == 0U || out_key_size == 0U || rounds == 0U) {
		return -1;
	}

	const size_t passphrase_len = strlen(passphrase);
	const size_t block_size = CS_CRYPTO_SHA256_SIZE;
	unsigned int block_count = (unsigned int)((out_key_size + block_size - 1U) / block_size);
	unsigned char u[CS_CRYPTO_SHA256_SIZE];
	unsigned char t[CS_CRYPTO_SHA256_SIZE];
	unsigned char salt_block[CS_CRYPTO_SALT_SIZE + 4U];

	if (salt_size > CS_CRYPTO_SALT_SIZE) {
		return -1;
	}

	for (unsigned int block_index = 1U; block_index <= block_count; ++block_index) {
		memcpy(salt_block, salt, salt_size);
		salt_block[salt_size + 0U] = (unsigned char)(block_index >> 24U);
		salt_block[salt_size + 1U] = (unsigned char)(block_index >> 16U);
		salt_block[salt_size + 2U] = (unsigned char)(block_index >> 8U);
		salt_block[salt_size + 3U] = (unsigned char)block_index;

		if (cs_kdf_hmac_sha256(passphrase, passphrase_len, salt_block, salt_size + 4U, u) != 0) {
			cs_memzero(u, sizeof(u));
			cs_memzero(t, sizeof(t));
			return -1;
		}
		memcpy(t, u, sizeof(t));

		for (unsigned int round = 1U; round < rounds; ++round) {
			if (cs_kdf_hmac_sha256(passphrase, passphrase_len, u, sizeof(u), u) != 0) {
				cs_memzero(u, sizeof(u));
				cs_memzero(t, sizeof(t));
				return -1;
			}
			for (size_t index = 0; index < sizeof(t); ++index) {
				t[index] ^= u[index];
			}
		}

		const size_t offset = (size_t)(block_index - 1U) * block_size;
		const size_t remain = out_key_size - offset;
		const size_t copy = remain < block_size ? remain : block_size;
		memcpy(out_key + offset, t, copy);
	}

	cs_memzero(u, sizeof(u));
	cs_memzero(t, sizeof(t));
	cs_memzero(salt_block, sizeof(salt_block));
	return 0;
}

int cs_kdf_derive_key(const char *passphrase,
	const unsigned char *salt,
	size_t salt_size,
	unsigned int rounds,
	unsigned char *out_key,
	size_t out_key_size) {
	return cs_kdf_pbkdf2_sha256(passphrase, salt, salt_size, rounds, out_key, out_key_size);
}

