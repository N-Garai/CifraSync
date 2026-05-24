# Project Scope

## Purpose
CifraSync is a command-line encrypted incremental backup and sync tool written in C.
It is intended to be a practical systems project for Windows-first development while keeping the core logic portable.

## Goals
- Back up only changed data by chunk hash.
- Restore snapshots reliably.
- Support compression and encryption.
- Resume interrupted work safely.
- Keep repository state on disk simple and auditable.

## Current Command Set
- `init` creates a repository layout.
- `backup` scans files, chunks data, stores new chunks, and writes snapshot metadata.
- `list` enumerates snapshots.
- `restore` reconstructs files from snapshot metadata.
- `verify` checks stored chunks against expected hashes.
- `prune` removes old snapshots according to retention policy.
- `sync` performs remote repository exchange over TCP.

## Non-Goals
- No GUI.
- No scripting language wrapper.
- No dependency on a large framework.
- No attempt to reproduce every rsync feature.

## Platform Direction
- Primary development environment: Windows 10/11 with PowerShell.
- Core code should stay OS-agnostic where possible.
- Windows-specific work belongs behind the platform abstraction layer.

## Deliverables
- A working local repository format.
- A stable backup and restore pipeline.
- Unit and integration test coverage for the current modules.
- Documentation that explains the current design and future roadmap.

## Definition of Done
A feature is complete when it handles valid and invalid inputs safely, produces useful diagnostics, and has at least one success case and one failure case covered by tests.
