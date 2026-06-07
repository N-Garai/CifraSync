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

	/* Round-trip with short plaintext */
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

	/* Wrong passphrase must fail */
	{
		unsigned char *enc_blob = NULL;
		size_t enc_size = 0U;
		if (cs_cipher_seal_alloc(passphrase, plain, sizeof(plain) - 1U, &enc_blob, &enc_size) != 0) {
			return cs_it_fail("seal failed for wrong-passphrase test");
		}
		if (cs_cipher_open_alloc("wrong passphrase", enc_blob, enc_size, &opened, &opened_size) == 0) {
			free(enc_blob);
			return cs_it_fail("wrong passphrase should have been rejected");
		}
		free(enc_blob);
	}

	/* Corrupted blob (tampered ciphertext) must fail authentication */
	{
		unsigned char *enc_blob = NULL;
		size_t enc_size = 0U;
		if (cs_cipher_seal_alloc(passphrase, plain, sizeof(plain) - 1U, &enc_blob, &enc_size) != 0) {
			return cs_it_fail("seal failed for corruption test");
		}
		enc_blob[cs_cipher_blob_overhead()] ^= 0xffU;
		if (cs_cipher_open_alloc(passphrase, enc_blob, enc_size, &opened, &opened_size) == 0) {
			free(enc_blob);
			return cs_it_fail("tampered blob should have been rejected");
		}
		free(enc_blob);
	}

	/* Truncated blob must fail */
	{
		unsigned char *enc_blob = NULL;
		size_t enc_size = 0U;
		if (cs_cipher_seal_alloc(passphrase, plain, sizeof(plain) - 1U, &enc_blob, &enc_size) != 0) {
			return cs_it_fail("seal failed for truncation test");
		}
		if (cs_cipher_open_alloc(passphrase, enc_blob, enc_size - 1U, &opened, &opened_size) == 0) {
			free(enc_blob);
			return cs_it_fail("truncated blob should have been rejected");
		}
		free(enc_blob);
	}

	/* Large payload (multi-block) */
	{
		size_t large_size = 100000U;
		unsigned char *large_plain = (unsigned char *)malloc(large_size);
		unsigned char *large_opened = NULL;
		size_t large_opened_size = 0U;

		if (large_plain == NULL) {
			return cs_it_fail("malloc failed for large payload test");
		}
		for (size_t i = 0U; i < large_size; ++i) {
			large_plain[i] = (unsigned char)(i & 0xffU);
		}

		if (cs_cipher_seal_alloc(passphrase, large_plain, large_size, &blob, &blob_size) != 0) {
			free(large_plain);
			return cs_it_fail("seal failed for large payload");
		}
		if (cs_cipher_open_alloc(passphrase, blob, blob_size, &large_opened, &large_opened_size) != 0) {
			free(large_plain);
			free(blob);
			return cs_it_fail("open failed for large payload");
		}
		free(blob);
		if (large_opened_size != large_size || memcmp(large_plain, large_opened, large_size) != 0) {
			free(large_plain);
			free(large_opened);
			return cs_it_fail("large payload roundtrip changed the plaintext");
		}
		free(large_plain);
		free(large_opened);
	}

	/* Empty plaintext */
	{
		unsigned char empty = 0;
		if (cs_cipher_seal_alloc(passphrase, &empty, 0U, &blob, &blob_size) != 0) {
			return cs_it_fail("seal failed for empty payload");
		}
		if (cs_cipher_open_alloc(passphrase, blob, blob_size, &opened, &opened_size) != 0) {
			free(blob);
			return cs_it_fail("open failed for empty payload");
		}
		free(blob);
		if (opened_size != 0U) {
			free(opened);
			return cs_it_fail("empty payload roundtrip should produce empty output");
		}
		free(opened);
	}

	/* Seal/open with null passphrase */
	if (cs_cipher_seal_alloc(NULL, plain, sizeof(plain) - 1U, &blob, &blob_size) == 0) {
		return cs_it_fail("seal with NULL passphrase should have failed");
	}

	return 0;
}
