# CifraSync

Windows-first C project for encrypted incremental backup and sync.

Current repository status:
- Full project scaffold is in place.
- Bootstrap build is wired through the Makefile.
- Main app and test runner are placeholders to validate toolchain setup.

## Quick Start (Windows)

1. Install GCC/Clang + make (MSYS2 MinGW-w64 recommended).
2. From this folder run:
	- `mingw32-make help`
	- `mingw32-make release`
	- `mingw32-make run`
	- `mingw32-make test`

If your environment provides `make`, you can use `make` instead.

## Important Docs

- `cifrasync_project_guide.md`: Detailed Windows-first architecture and roadmap.
- `cbackup_requirements.md`: Original requirements draft.
