# Roadmap

## Current State
All 7 commands implemented (init, backup, restore, list, verify, prune, sync). Compression (RLE) and encryption (HMAC stream cipher v2 with key separation, CSPRNG, 600K PBKDF2) are wired end-to-end. File-based mutual exclusion locks protect concurrent access. Remote sync server + client work with manifest/chunk transfer. 14 tests pass with zero warnings.

## Completed Milestones
- All commands functional with interactive TUI and CLI
- Compression + encryption end-to-end (compress-then-encrypt, decrypt-then-decompress)
- Encryption hardened: key separation, CSPRNG salt/nonce, 600K PBKDF2 iterations, masked input, key cache
- Wrong-passphrase detection on restore/verify (no silent garbage)
- File-based mutual exclusion across all operations
- Cross-platform: Windows (MinGW), Linux (gcc), macOS (clang)
- Full test suite: 9 unit + 5 integration tests
- Comprehensive docs: README, project scope, repo format, wire protocol, security model, test strategy

## Near-Term Priorities
1. Switch to a memory-hard KDF (Argon2id) for GPU/ASIC resistance.
2. Add TLS for remote sync transport.
3. Improve performance on large directory trees (parallel chunk processing).

## Mid-Term Priorities
1. Add concurrency and pipeline parallelism in backup.
2. Add non-interactive key-file or env-var passphrase support.
3. Improve error messages and diagnostics.

## Long-Term Priorities
1. Add cloud storage backend support (S3, Backblaze B2).
2. Add file-watching daemon mode for continuous backup.
3. Add GUI or web dashboard.

## Documentation Maintenance
- Keep docs aligned with current repository layout and features.
- Keep security model current with implemented cryptography.
- Keep test strategy aligned with actual runner files.
