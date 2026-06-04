# CifraSync

Encrypted Incremental Backup & Sync — written in C.

## Quick Start

```bash
# Build & run (interactive mode)
mingw32-make release
cifrasync

# Or use the launcher
wake_up_cifra          # cmd.exe
.\wake_up_cifra.cmd    # PowerShell
```

After one-time setup (`powershell -ExecutionPolicy Bypass -File setup.ps1`):
```bash
wake up cifra          # works from any terminal
```

## Commands

| Command    | Description                                  | Options |
|------------|----------------------------------------------|---------|
| `init`     | Initialize a repository                      | `--repo PATH` |
| `backup`   | Create an incremental backup                 | `--source PATH --repo PATH [--compress] [--encrypt] [--label TEXT] [--dry-run] [--include-file FILE] [--exclude-file FILE]` |
| `list`     | List snapshots with details                  | `--repo PATH` |
| `restore`  | Restore from a snapshot                      | `--repo PATH --snapshot ID --out PATH [--source-file PATH]` |
| `verify`   | Verify stored data integrity                 | `--repo PATH` |
| `prune`    | Remove old snapshots                         | `--repo PATH [--keep-last N] [--older-than DAYS]` |
| `sync`     | Synchronize with remote repository           | `--repo PATH --remote HOST:PORT` |

Running `cifrasync` with no arguments launches the **interactive menu** with a colored TUI.

## Features

- **Chunk-based deduplication** (SHA-256, 1 MiB fixed-size chunks)
- **RLE compression** (`--compress`) — chunks are compressed before storage, auto-decompressed on restore/verify
- **HMAC stream cipher encryption** (`--encrypt`) — passphrase-based, chunks sealed before storage
- **TCP remote sync** — full client/server protocol with frame-based messaging
- **Include/exclude pattern files** (`--include-file`, `--exclude-file`)
- **Rich snapshot listing** — ID, human timestamp, file count, human-readable size, label
- **Single-file restore** (`--source-file PATH`)
- **Data integrity verify** — re-reads every chunk, recomputes SHA-256, reports missing/corrupt
- **Snapshot prune** — keep-last N, older-than N days, orphan chunk GC
- **Atomic manifest writes** — `.tmp` + `rename()` for crash safety
- **Journal replay** — operation journal auto-cleared on startup
- **Config auto-load** — `cifrasync.conf` loaded at startup if present
- **Interactive menu** — colored ANSI TUI, prompts for all options

## Build

```bash
mingw32-make release    # optimized build  (-O2)
mingw32-make debug      # debug build       (-O0 -g3)
mingw32-make test       # build & run tests (13 tests)
mingw32-make run        # build & launch interactive mode
mingw32-make tools      # utility programs
mingw32-make clean      # remove build artifacts
```

Requires GCC + make (MinGW-w64 on Windows, gcc + make on Linux/macOS).

## Project Structure

```
src/        — Source files by module (cli/ core/ crypto/ delta/ fs/ net/ storage/ util/ common/ compress/)
include/    — Header files matching src/ layout
tests/      — 9 unit + 4 integration tests
tools/      — wake, benchmark_hash, corrupt_chunk, gen_test_tree
docs/       — Design documentation
examples/   — Sample include/exclude pattern files
```

## Platform Support

| Platform | Status |
|----------|--------|
| Windows (MinGW-w64) | Primary target, fully tested |
| Linux (gcc) | Builds with zero changes, POSIX fallbacks implemented |
| macOS (clang) | Should build, not tested |

Key cross-platform features: `#ifdef _WIN32` / `#else` for all system calls, POSIX `opendir`/`readdir` fallbacks for directory walking, pthread alternative for thread pool, platform-aware Makefile.

## Docs

- `cifrasync_project_guide.md` — Architecture and roadmap
- `cbackup_requirements.md` — Original requirements
- `after.md` — Session progress log
- `update_guide.md` — Phase planning
