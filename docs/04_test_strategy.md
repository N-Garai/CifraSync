# Test Strategy

## Goal
Tests should verify the current modules and keep the backup flow stable as the project grows.

## Current Test Layout
- `tests/main_test.c` is the active test runner.
- Unit tests live under `tests/unit/`.
- Integration tests live under `tests/integration/`.
- Utility programs live under `tools/` and can be used for manual checks.

## Unit Coverage
The unit suite should cover:
- CLI parsing
- hashing
- chunking
- compression
- encryption
- repository setup
- index store behavior
- snapshot store behavior
- filesystem scanning

## Integration Coverage
The integration suite should cover:
- backup and restore round trips
- incremental resume behavior
- remote sync over localhost
- verify and prune behavior

## Temporary Data
- Integration tests should create temporary source and repository roots.
- Temporary repos should be isolated from the workspace repo root.
- Cleanup should happen even on failure when possible.

## Quality Gates
- `mingw32-make release`
- `mingw32-make test`
- At least one manual smoke run for repository initialization or backup flow

## Failure Rules
- A failing test should report the smallest useful error message.
- Tests should not depend on external infrastructure.
- Tests should prefer deterministic data and short-lived temp paths.

## Manual Support Tools
- `tools/gen_test_tree.c` generates repeatable sample trees.
- `tools/benchmark_hash.c` measures hashing throughput.
- `tools/corrupt_chunk.c` can simulate damaged repository data for verification testing.
