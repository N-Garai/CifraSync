# Repository Format

## Overview
The repository is a directory tree that stores chunks, indexes, snapshots, journal state, and lock metadata.
The format is designed so the backup source tree stays outside the repository root.

## Expected Layout
```text
<repo>/
	repo.meta
	chunks/
		ab/
			abcd1234...chunk
	snapshots/
		2026-04-13T10-22-00Z.snapshot
	index/
		chunks.idx
	journal/
		active.journal
	locks/
		repo.lock
```

## Directory Responsibilities
- `repo.meta`: repository identity and version information.
- `chunks/`: chunk payloads stored by hash fanout.
- `snapshots/`: snapshot metadata files for restore and listing.
- `index/`: lookup data that maps chunk hashes to stored chunk locations.
- `journal/`: append-only records for resumable operations.
- `locks/`: mutual exclusion and active operation markers.

## Chunk Storage Rules
- Chunk names should derive from the chunk hash.
- Hash-prefix fanout keeps directories manageable.
- Chunk payloads should never be stored under source tree names.

## Snapshot Rules
- A snapshot records file paths, sizes, chunk references, and metadata needed for restore.
- Snapshot metadata must refer to chunk hashes, not raw source blobs.
- Snapshot filenames should be safe for Windows path rules.

## Journal Rules
- Journal writes are append-only.
- Journal records should be flushed before a state transition is considered durable.
- Finalization should be atomic when possible.

## Notes for Tests
- Integration tests use temporary repository roots under the system temp directory.
- A repo fixture such as `test_repo` is optional and only useful for manual smoke testing.
