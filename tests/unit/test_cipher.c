#include "../integration/test_support.h"

#include "crypto/cipher.h"

int cs_unit_test_cipher(void) {
	static const char *passphrase = "correct horse battery staple";
	static const unsigned char plain[] = "encrypted payload for unit testing";
	unsigned char *blob = NULL;
	size_t blob_size = 0U;
	unsigned char *opened = NULL;
	size_t opened_size = 0U;

	if (cs_cipher_blob_overhead() == 0U) {
		return cs_it_fail("cipher overhead should never be zero");
	}
	if (cs_cipher_seal_alloc(passphrase, plain, sizeof(plain) - 1U, &blob, &blob_size) != 0) {
		return cs_it_fail("cipher seal failed");
	}
	if (cs_cipher_open_alloc(passphrase, blob, blob_size, &opened, &opened_size) != 0) {
		free(blob);
		return cs_it_fail("cipher open failed");
	}
	free(blob);
	if (opened_size != sizeof(plain) - 1U || memcmp(opened, plain, opened_size) != 0) {
		free(opened);
		return cs_it_fail("cipher roundtrip changed the plaintext");
	}
	free(opened);

	return 0;
}
