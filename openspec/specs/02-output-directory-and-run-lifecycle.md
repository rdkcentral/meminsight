# C02 - Output directory initialization and run lifecycle (As-Is)

## Scope

This specification defines how meminsight prepares output directories and manages per-run lifecycle files.

## Current behavior

1. At run setup, the configured output directory is ensured to exist.
2. If the output directory exists and is a directory, pre-run retention is applied only to report files that match the active format (`.csv` in CSV mode, `.json` in JSON mode).
3. If matching report count is less than or equal to retention count, all matching reports are moved to a timestamped retention directory under the output directory.
4. If matching report count exceeds retention count, newest `N` matching reports are moved to retention and older matching reports are removed.
5. Non-matching files in the output directory are not modified by retention processing.
6. If the target path exists but is not a directory, run setup fails.
7. If the directory does not exist, a single-level create is attempted.
8. Each run creates an in-progress sentinel file before collection starts.
9. The in-progress sentinel file is removed on normal completion and on handled error exits.

## Safety behavior

1. Retention file operations use descriptor-relative operations (renameat/unlinkat/mkdirat) to reduce path truncation and race hazards.
2. Archive directory naming handles same-second collisions by retrying with suffixed names.

## Source anchors

- `ensure_output_dir()`, `clear_dir_contents()`, `clear_dir_fd()`, `touchFile()`, `removeFileIfPresent()`, and `initializeSetupInfo()` in [src/meminsight.c](../../src/meminsight.c)
