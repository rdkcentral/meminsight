# C02 - Output directory initialization and run lifecycle (As-Is)

## Scope

This specification defines how meminsight prepares output directories and manages per-run lifecycle files.

## Current behavior

1. At run setup, the configured output directory is ensured to exist.
2. The final path component of the output directory must contain `meminsight`; otherwise run setup fails.
3. If the output directory exists and is a directory, pre-run backup handling is applied only to report files that match the active format (`.csv` in CSV mode, `.json` in JSON mode).
4. If matching report count is less than or equal to backup count, all matching reports are moved to a timestamped backup directory named `<timestamp>_<RUN_ID>_backup` under the output directory.
5. If matching report count exceeds backup count, newest `N` matching reports are moved to backup and older matching reports are removed.
6. Non-matching files in the output directory are not modified by backup processing.
7. If the target path exists but is not a directory, run setup fails.
8. If the directory does not exist, a single-level create is attempted.
9. Each run creates an in-progress sentinel file before collection starts.
10. The in-progress sentinel file is removed on normal completion and on handled error exits.

## Safety behavior

1. Backup file operations use descriptor-relative operations (renameat/unlinkat/mkdirat) to reduce path truncation and race hazards.
2. Archive directory naming handles same-second collisions by retrying with suffixed names.

## Source anchors

- `ensure_output_dir()`, `apply_backup_policy()`, `touchFile()`, `removeFileIfPresent()`, and `initializeSetupInfo()` in [src/meminsight.c](../../src/meminsight.c)
