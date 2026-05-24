# Security Model

## Overview
CifraSync protects data at rest and in transit while preserving repository integrity.
The current design favors simple, explicit security boundaries over hidden magic.

## Data at Rest
- Chunk data should be encrypted before it is written to disk when encryption is enabled.
- Repository metadata should avoid leaking plaintext source content where practical.
- Keys should never be stored in plaintext.

## Data in Transit
- Remote sync traffic should use a framed protocol with integrity checks.
- Transport security can be strengthened later, but the protocol must already validate message boundaries and lengths.

## Key Handling
- Derive encryption material from a passphrase.
- Zero temporary key buffers after use.
- Avoid logging secrets or passphrases.

## Integrity
- Chunk hashes are used to detect corruption and deduplicate storage.
- Snapshot metadata should reference hashes, not raw payload copies.
- Verification should recompute expected hashes from stored data.

## Path Safety
- Normalize and validate paths before file creation or restore.
- Reject parent-directory traversal where it would escape the intended root.
- Keep repository paths and restore output paths separate.

## Operational Safety
- Use atomic updates for metadata when possible.
- Flush journal records before treating a step as durable.
- Fail closed on malformed input rather than trying to recover silently.

## Threats Addressed
- Accidental data loss from interrupted backups.
- Corruption caused by partial writes.
- Repository tampering detected by verify routines.
- Path traversal in restore and remote inputs.

## Remaining Work
- Stronger authenticated remote transport.
- More detailed access-control controls.
- Additional hardening around configuration and key management.
