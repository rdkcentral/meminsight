# C02 - Output directory initialization and run lifecycle (As-Is)

## Scope

This specification defines how meminsight prepares output directories and manages per-run lifecycle files.

## Current behavior

1. At run setup, the configured output directory is ensured to exist.
2. The output path must not contain a `..` component, and the final path component must contain `meminsight`; otherwise run setup fails.
3. Directory creation uses `mkdir` first. `EEXIST` is treated as already present, then the path is opened as a directory. This avoids a stat-then-create race.
4. If the output directory already exists and is a directory, pre-run backup handling is applied only to report files that match the active format (`.csv` in CSV mode, `.json` in JSON mode).
5. If matching report count is less than or equal to backup count, all matching reports are moved to a timestamped backup directory named `<timestamp>_<RUN_ID>_backup` under the output directory.
6. If matching report count exceeds backup count, newest `N` matching reports are moved to backup and older matching reports are removed.
7. Non-matching files in the output directory are not modified by backup processing.
8. If the target path exists but is not a directory, run setup fails.
9. If the directory does not exist, a single-level create is attempted.
10. Each run creates an in-progress sentinel file before collection starts.
11. The in-progress sentinel file is removed on normal completion and on handled error exits.
12. `RUN_ID` is generated once at setup as `<epoch_seconds><pid><2-digit-suffix>`. The suffix is read from `/dev/urandom` when available, otherwise mixed from time and pid. `rand()` is not used.

## Safety behavior

1. Backup file operations use descriptor-relative operations (renameat/unlinkat/mkdirat) to reduce path truncation and race hazards.
2. Archive directory naming handles same-second collisions by retrying with suffixed names.
3. Per-iteration report files are opened with `fopen` after the output directory has already been validated at startup, so the collect loop does not add extra path-walk or permission syscalls.

## Source anchors

- `ensure_output_dir()`, `apply_backup_policy()`, `touchFile()`, `removeFileIfPresent()`, and `initializeSetupInfo()` in [src/meminsight.c](../../src/meminsight.c)
