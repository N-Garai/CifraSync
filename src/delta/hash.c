#include "delta/hash.h"

#include "crypto/kdf.h"
#include "common/memory.h"

#include <ctype.h>
#include <string.h>

static int cs_hex_nibble(int ch) {
	if (ch >= '0' && ch <= '9') {
		return ch - '0';
	}
	if (ch >= 'a' && ch <= 'f') {
		return 10 + (ch - 'a');
	}
	if (ch >= 'A' && ch <= 'F') {
		return 10 + (ch - 'A');
	}
	return -1;
}

int cs_hash_bytes_to_hex(const unsigned char *bytes, size_t byte_count, char *out_hex, size_t out_size) {
	static const char digits[] = "0123456789abcdef";

	if (bytes == NULL || out_hex == NULL) {
		return -1;
	}
	if (out_size < (byte_count * 2U) + 1U) {
		return -1;
	}

	for (size_t index = 0U; index < byte_count; ++index) {
		out_hex[index * 2U] = digits[(bytes[index] >> 4U) & 0x0fU];
		out_hex[index * 2U + 1U] = digits[bytes[index] & 0x0fU];
	}
	out_hex[byte_count * 2U] = '\0';
	return 0;
}

int cs_hash_hex_to_bytes(const char *hex, unsigned char *out_bytes, size_t out_size) {
	size_t hex_len;

	if (hex == NULL || out_bytes == NULL) {
		return -1;
	}

	hex_len = strlen(hex);
	if ((hex_len % 2U) != 0U || (hex_len / 2U) > out_size) {
		return -1;
	}

	for (size_t index = 0U; index < hex_len; index += 2U) {
		int hi = cs_hex_nibble((unsigned char)hex[index]);
		int lo = cs_hex_nibble((unsigned char)hex[index + 1U]);
		if (hi < 0 || lo < 0) {
			return -1;
		}
		out_bytes[index / 2U] = (unsigned char)((hi << 4U) | lo);
	}

	return 0;
}

bool cs_hash_is_hex(const char *hex) {
	if (hex == NULL || hex[0] == '\0') {
		return false;
	}
	for (size_t index = 0U; hex[index] != '\0'; ++index) {
		if (cs_hex_nibble((unsigned char)hex[index]) < 0) {
			return false;
		}
	}
	return true;
}

int cs_hash_sha256(const void *data, size_t data_size, unsigned char out_hash[CS_HASH_SHA256_BYTES]) {
	if (out_hash == NULL || (data == NULL && data_size > 0U)) {
		return -1;
	}
	return cs_kdf_sha256(data, data_size, out_hash);
}

int cs_hash_sha256_hex(const void *data, size_t data_size, char out_hex[CS_HASH_HEX_BUFSZ]) {
	unsigned char hash[CS_HASH_SHA256_BYTES];

	if (cs_hash_sha256(data, data_size, hash) != 0) {
		return -1;
	}
	if (cs_hash_bytes_to_hex(hash, sizeof(hash), out_hex, CS_HASH_HEX_BUFSZ) != 0) {
		return -1;
	}
	cs_memzero(hash, sizeof(hash));
	return 0;
}

