#include "../integration/test_support.h"

#include "delta/hash.h"

int cs_unit_test_hash(void) {
	static const char *expected_hex = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
	unsigned char bytes[CS_HASH_SHA256_BYTES];
	char hex[CS_HASH_HEX_BUFSZ];
	unsigned char roundtrip[CS_HASH_SHA256_BYTES];

	if (cs_hash_sha256_hex("abc", 3U, hex) != 0) {
		return cs_it_fail("sha256 hex computation failed");
	}
	if (strcmp(hex, expected_hex) != 0) {
		return cs_it_fail("sha256 hex value did not match the known test vector");
	}
	if (!cs_hash_is_hex(hex)) {
		return cs_it_fail("hash validator rejected a valid hex string");
	}
	if (cs_hash_hex_to_bytes(hex, bytes, sizeof(bytes)) != 0) {
		return cs_it_fail("hash hex to bytes conversion failed");
	}
	if (cs_hash_bytes_to_hex(bytes, sizeof(bytes), hex, sizeof(hex)) != 0) {
		return cs_it_fail("hash bytes to hex conversion failed");
	}
	if (strcmp(hex, expected_hex) != 0) {
		return cs_it_fail("hash roundtrip changed the encoded value");
	}
	if (cs_hash_hex_to_bytes(hex, roundtrip, sizeof(roundtrip)) != 0) {
		return cs_it_fail("second hash roundtrip failed");
	}
	if (memcmp(bytes, roundtrip, sizeof(bytes)) != 0) {
		return cs_it_fail("hash roundtrip bytes did not match");
	}

	return 0;
}
