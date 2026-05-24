# Roadmap

## Current State
The core codebase already has a working local backup and restore path, a combined test runner, and utility tools for smoke testing.

## Near-Term Priorities
1. Tighten repository initialization and list behavior.
2. Improve documentation for repository and protocol details.
3. Add richer restore and verify command handling.
4. Expand remote sync behavior and error reporting.

## Mid-Term Priorities
1. Add stronger platform abstraction for filesystem and networking.
2. Improve encryption and compression integration.
3. Add journaling/replay robustness.
4. Reduce platform-specific assumptions in lower layers.

## Long-Term Priorities
1. Add concurrency and pipeline parallelism.
2. Improve performance on large trees.
3. Add better transport security.
4. Harden against malformed input and partial failures.

## Suggested Implementation Order
1. Finish command-level behavior.
2. Improve repository metadata and listing.
3. Expand restore, verify, and prune paths.
4. Add sync retries and protocol validation.
5. Polish logging and diagnostics.

## Documentation Maintenance
- Update docs when repository layout changes.
- Keep test strategy aligned with actual runner files.
- Keep security notes aligned with the current threat model.

## Success Criteria
The project is moving in the right direction when `release`, `test`, and manual smoke scenarios continue to work together without the docs drifting away from the code.
