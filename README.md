# CifraSync

**Encrypted Incremental Backup & Sync** — a fast, cross-platform backup tool written in C from scratch.

---

## Why CifraSync?

Most backup tools fall into two camps:

- **Heavyweight** (Duplicati, Restic, Borg) — powerful but complex to configure, written in high-level languages, require runtime dependencies.
- **Too simple** (robocopy, rsync) — no deduplication, no encryption, no incremental snapshot management.

CifraSync fills the gap: a **single small binary**, zero dependencies, with chunk-based deduplication, streaming encryption, incremental snapshots, and remote sync. It is designed for developers, homelab users, and anyone who wants a reliable backup tool they can build from source in seconds.

## Novelty

| Feature | Why It Stands Out |
|---|---|
| **Pure C, zero dependencies** | Single statically-linked binary; no Python, no Go runtime, no npm. Builds in <5 seconds. |
| **Chunk-level deduplication** | Fixed-size 1 MiB SHA-256 chunks; identical files or repeated data across snapshots stored only once. |
| **RLE compression + HMAC cipher (v2)** | Per-chunk compress-then-encrypt pipeline. Blob v2 with key separation (enc_key/mac_key derived via HMAC), cryptographically random salt+nonce, 600K PBKDF2 iterations, and constant-time tag verification. |
| **Atomic manifest writes** | `.tmp` + `rename()` — snapshots are never left in a half-written state, even on power loss. |
| **Journal replay** | Operation journal is replayed on startup; interrupted backups auto-resume without corruption. |
| **Cross-platform by design** | Single codebase compiles on Windows (MinGW), Linux (gcc), and macOS (clang) with `#ifdef` for platform differences. |
| **Rich interactive TUI** | Colored menu-driven interface — no CLI flags needed for day-to-day use. |

---

## Quick Start

```bash
# Clone repo
git clone https://github.com/N-Garai/CifraSync.git
cd CifraSync

# Build
mingw32-make release

# Run directly
cifrasync

# Run via launcher script
./wake_up_cifra.cmd        # cmd.exe
.\wake_up_cifra.cmd         # PowerShell
./wake_up_cifra.sh          # Linux/macOS
```

After one-time setup:
```bash
wake up cifra
```

### One-Time Setup

```powershell
powershell -ExecutionPolicy Bypass -File setup.ps1
```

This adds `cifrasync` to PATH and defines the `wake up cifra` PowerShell function. Now you can run `wake up cifra` from any terminal.

---

## Interactive Mode

When `cifrasync` is run with no arguments, it launches a colored interactive TUI. Enter a number to execute a command:

```
  +-----------------------------------------+
  |         C I F R A S Y N C               |
  |    Encrypted Incremental Backup & Sync  |
  +-----------------------------------------+

  repo: C:\Users\...\my_repo

  OK Ready

  1  init     Initialize repository
  2  backup   Incremental backup
  3  list     List snapshots
  4  restore  Restore from snapshot
  5  verify   Verify integrity
  6  prune    Remove old snapshots
  7  sync     Remote sync
  8  serve    Start server
  9  help     Show help
  0  exit     Quit

  >> Choice (0-9):
```

### Option Reference

| # | Command | Description | Prompts | Example Input |
|---|---|---|---|---|
| 1 | `init` | Create repo directory structure | None (uses repo path set at startup) | — |
| 2 | `backup` | Create an incremental snapshot | **Source path** — directory to back up | `C:\Users\me\Documents` |
| | | | **Label** — optional name for this snapshot | `weekly`, `before-upgrade` |
| | | | **Include file** — file with glob patterns to include | `C:\include.txt` (blank = all) |
| | | | **Exclude file** — file with glob patterns to skip | `C:\exclude.txt` (blank = none) |
| | | | **Compress** — `y`/`n` to RLE-compress chunks | `y` |
| | | | **Encrypt** — `y`/`n` to encrypt with passphrase (masked input, no echo) | `n` |
| | | | **Dry run** — `y`/`n` to scan without storing | `n` |
| 3 | `list` | List all snapshots | None | — |
| 4 | `restore` | Restore files from snapshot | **Snapshot ID** — ID from `list` output | `2026-06-04T15-28-06Z` |
| | | | **Output path** — where to write restored files | `C:\restored` |
| | | | **Single file** — restore only one path | `.gitignore` (blank = all) |
| 5 | `verify` | Check chunk integrity (passphrase for encrypted repos) | None (passphrase prompted if repo encrypted) | — |
| 6 | `prune` | Remove old snapshots + orphan chunks | **Keep last N** — keep N newest snapshots | `7` |
| | | | **Older than N days** — delete snapshots older than N days | `30` (blank = off) |
| 7 | `sync` | Sync with remote server (full manifest + chunk transfer) | **Remote host:port** — server address | `192.168.1.100:9000` |
| 8 | `serve` | Start server for incoming sync connections | **Bind address** — host:port to listen on | `0.0.0.0:9000` |
| | | | **Repo path** — repository to receive data into | `C:\repo` (blank = ACK-only, no storage) |
| 9 | `help` | Show CLI help text | None | — |
| 0 | `exit` | Quit | None | — |

---

## CLI Reference

All interactive commands are also available via CLI flags:

```bash
cifrasync init --repo PATH
cifrasync backup --source PATH --repo PATH [--compress] [--encrypt] [--label TEXT] [--dry-run] [--include-file FILE] [--exclude-file FILE]
cifrasync list --repo PATH
cifrasync restore --repo PATH --snapshot ID --out PATH [--source-file PATH]
cifrasync verify --repo PATH
cifrasync prune --repo PATH [--keep-last N] [--older-than DAYS]
cifrasync sync --repo PATH --remote HOST:PORT
cifrasync serve --bind HOST:PORT [--repo PATH]
```

---

## Features

| Feature | Status | Details |
|---|---|---|
| Chunk-based deduplication | Done | SHA-256, 1 MiB fixed-size chunks |
| RLE compression | Done | Per-chunk, auto-decompressed on restore/verify |
| Stream cipher encryption (v2) | Done | HMAC-based, key separation (enc_key/mac_key), CSPRNG salt+nonce, 600K PBKDF2 iterations, masked passphrase input |
| Incremental snapshots | Done | Only new/changed files stored each run |
| Snapshot list with details | Done | ID, timestamp, file count, human size, label |
| Single-file restore | Done | `--source-file PATH` |
| Full directory restore | Done | Restores complete file tree from snapshot |
| Data integrity verify | Done | Re-reads all chunks, recomputes SHA-256 |
| Snapshot prune | Done | `--keep-last N`, `--older-than DAYS` |
| Orphan chunk GC | Done | Deletes unreferenced chunks after prune |
| Remote TCP sync | Done | Full manifest + chunk data transfer; incremental (only sends missing chunks) |
| Include/exclude files | Done | Glob-pattern-based file filtering |
| Atomic manifest writes | Done | `.tmp` + `rename()` |
| Journal replay | Done | Auto-resume on interrupted backup |
| Config auto-load | Done | `cifrasync.conf` loaded on startup if present |
| Interactive TUI | Done | Colored numbered menu |
| "wake up cifra" | Done | One-command launcher + PATH setup |
| Cross-platform | Done | Windows MinGW / Linux gcc / macOS clang |
| File-based mutual exclusion | Done | EXCLUSIVE (backup/prune/sync-server) / SHARED (restore/verify/sync-client) locks via flock / LockFileEx |

---

## Architecture

```
src/
├── main.c              — Entry point; launches interactive or CLI
├── interactive.c        — Interactive TUI menu
├── cli/
│   ├── commands.c      — CLI dispatcher, config load
│   └── parser.c        — Argument parsing
├── core/
│   ├── engine.c        — Backup/restore/verify/prune/sync orchestration
│   ├── planner.c       — File change planning
│   └── journal.c       — Crash-safe operation journal
├── storage/
│   ├── repo.c          — Repository init/layout
│   ├── chunk_store.c   — Chunk read/write
│   ├── index_store.c   — Hash index
│   ├── snapshot_store.c— Snapshot metadata
│   └── lock.c          — File-based mutual exclusion
├── delta/
│   ├── hash.c          — SHA-256 wrapper
│   ├── chunker.c       — File-to-chunk splitting
│   └── manifest.c      — Snapshot manifest
├── crypto/
│   ├── cipher.c        — HMAC stream cipher
│   ├── kdf.c           — Key derivation (PBKDF2-like)
│   └── key_cache.c     — In-memory key cache
├── compress/
│   └── codec.c         — RLE compression
├── fs/
│   ├── scanner.c       — Directory tree scanner
│   ├── metadata.c      — File metadata
│   └── file_reader.c   — Chunked file reader
├── net/
│   ├── client.c        — TCP sync client
│   ├── server.c        — TCP sync server
│   └── protocol.c      — Frame-based wire protocol
├── util/
│   ├── config.c        — Config file parser
│   ├── thread_pool.c   — Thread pool (pthread/Windows)
│   ├── time_utils.c    — Timestamp formatting
│   └── io_utils.c      — File I/O helpers
└── common/
    ├── errors.c        — Error code to string
    ├── path.c          — Path normalization
    ├── log.c           — Structured logging
    └── memory.c        — Safe alloc wrappers
```

### Repo Layout

```
<repo>/
├── chunks/        — Raw chunk files (named by SHA-256 hash)
├── snapshots/     — Snapshot manifest files (.snapshot)
├── index/         — Hash → chunk mapping index
├── journal/       — Operation journal for crash recovery
└── locks/         — File-based mutual exclusion locks
```

---

## Build

```bash
mingw32-make release    # optimized (-O2)
mingw32-make debug      # debug (-O0 -g3)
mingw32-make asan       # AddressSanitizer + UBSan
mingw32-make test       # build + run all tests (14 tests)
mingw32-make run        # build release + launch interactive
mingw32-make tools      # utility programs (wake, benchmark_hash, etc.)
mingw32-make clean      # remove build/ and bin/
```

**Requirements**: GCC + make (MinGW-w64 on Windows, gcc + make on Linux/macOS).

---

## Platform Support

| Platform | Status | Notes |
|---|---|---|
| Windows (MinGW-w64) | **Fully tested** | All 14 tests pass; primary target |
| Linux (gcc) | **Builds clean** | POSIX fallbacks for walk/GC/verify/list; not CI-tested |
| macOS (clang) | **Should build** | Shares Linux code path; not tested |

---

## Testing

14 tests total (9 unit + 5 integration):

- **Unit**: parser, hash, chunker, codec, cipher, repo, index_store, snapshot_store, scanner
- **Integration**: backup+restore round-trip, encrypted backup+restore, incremental resume, remote sync, verify+prune

```bash
mingw32-make test
```

---

## Docs

| Document | Description |
|---|---|
| `docs/00_project_scope.md` | Project goals and scope |
| `docs/01_repository_format.md` | On-disk repo format specification |
| `docs/02_wire_protocol.md` | Remote sync protocol |
| `docs/03_security_model.md` | Encryption and key derivation |
| `docs/04_test_strategy.md` | Testing approach |
| `docs/05_roadmap.md` | Future development plans |