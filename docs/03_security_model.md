# Security Model

## Overview
CifraSync protects data at rest and in transit while preserving repository integrity.
The current design favors simple, explicit security boundaries over hidden magic.

## Data at Rest
- Chunk data is encrypted before being written to disk when encryption is enabled.
- Encryption uses HMAC-SHA256 in CTR-like mode as a stream cipher.
- Blob v2 with **key separation**: enc_key for stream cipher, mac_key for authentication tag, derived via HMAC-SHA256 from the PBKDF2 master key.
- **600,000 PBKDF2-HMAC-SHA256 iterations** (OWASP 2023 recommended minimum).
- **Cryptographically random salt (16 bytes) and nonce (16 bytes)** per encrypted blob — generated via CryptGenRandom (Windows) or /dev/urandom (POSIX).
- **Authenticated encryption (Encrypt-then-MAC)** — ciphertext is integrity-checked before decryption via constant-time HMAC tag verification.
- **Wrong-passphrase detection** — tampered or incorrectly decrypted blobs are rejected with an error, never silently producing garbage.
- **Blob versioning** — version field in header enables forward-compatible algorithm upgrades.
- Repository metadata does not leak plaintext source content.
- Keys are never stored in plaintext; passphrase is zeroized on stack after use.
- **In-memory key cache** — derived keys cached per-session to avoid redundant PBKDF2.

## Data in Transit
- Remote sync traffic uses a framed protocol with integrity checks (SHA-256 hashes + length-prefixed frames).
- Transport security can be strengthened later, but the protocol validates message boundaries and lengths.

## Key Handling
- All encryption material derived from a passphrase via PBKDF2-HMAC-SHA256 (600K iterations).
- Master key split into enc_key + mac_key via HMAC derivation (key separation).
- Temporary key buffers zeroized after use with `cs_memzero`.
- Passphrase input is masked (no echo) — SetConsoleMode on Windows, getpass on POSIX.
- Passphrase stack buffer is cleared in backup cleanup path.
- No secrets or passphrases are logged.

## Integrity
- Chunk hashes (SHA-256) detect corruption and enable deduplicated storage.
- Each encrypted blob carries a 32-byte HMAC-SHA256 authentication tag, verified with constant-time comparison before decryption.
- Verification recomputes expected hashes from decrypted+decompressed data.
- Corrupt or tampered chunks are detected and reported.

## Operational Safety
- **File-based mutual exclusion** — EXCLUSIVE lock for write operations (backup, prune, server sync), SHARED lock for read operations (restore, verify, client sync). Cross-platform via flock (POSIX) / LockFileEx (Windows).
- Atomic manifest writes (`.tmp` + `rename()`).
- Journal records flushed before treating a step as durable.
- Fail closed on malformed input rather than trying to recover silently.

## Threats Addressed
- Accidental data loss from interrupted backups.
- Corruption caused by partial writes or concurrent access.
- Repository tampering detected by verify routines and authentication tags.
- Path traversal in restore and remote inputs.
- Nonce reuse (prevented by CSPRNG nonce generation).
- Offline brute-force (mitigated by 600K PBKDF2 iterations).
- Memory disclosure of passphrase (mitigated by stack zeroization).

## Remaining Work
- Stronger authenticated remote transport (TLS).
- More detailed access-control controls.
- Switch to a memory-hard KDF (Argon2id) for additional GPU/ASIC resistance.
- Multi-key or key-file support for non-interactive backup scenarios.
