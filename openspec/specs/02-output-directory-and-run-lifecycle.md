# C02 - Output directory initialization and run lifecycle (As-Is)

## Scope

This specification defines how meminsight prepares output directories and manages per-run lifecycle files.

## Current behavior

1. At run setup, the configured output directory is ensured to exist.
2. If the output directory already exists and is a directory, existing contents are recursively removed before new report generation.
3. If the target path exists but is not a directory, run setup fails.
4. If the directory does not exist, a single-level create is attempted.
5. Each run creates an in-progress sentinel file before collection starts.
6. The in-progress sentinel file is removed on normal completion and on handled error exits.

## Safety behavior

1. Recursive directory cleanup uses descriptor-relative operations to reduce race hazards during deletion.
2. Cleanup continues on per-entry removal errors and returns overall status.

## Source anchors

ensure_output_dir(), clear_dir_contents(), clear_dir_fd(), touchFile(), removeFileIfPresent(), initializeSetupInfo() in src/meminsight.c
