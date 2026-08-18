# C11 - Upload marker and configstore state file behavior (As-Is)

## Scope

This specification defines the current upload signaling and run-state file behavior.

## Files

- /tmp/.meminsight_upload
- `<output-dir>/.meminsight_configstore`
- /tmp/.meminsight_inprogress

## Current behavior

1. At run startup, the configstore is written or updated only in the selected output directory.
2. If configstore already contains the same persistent key/value set, file rewrite is skipped.
3. The configstore retains `RUN_ID` for backup archive continuity, but does not store upload-only settings.
4. When `--upload-enable` is supplied, an atomic upload marker is published after configstore creation and before capture begins.
5. The marker contains `CONFIGSTORE_PATH`, `RUN_ID`, `UPLOAD_ENABLED`, and `UPLOAD_INTERVAL`.
6. The sidecar reads upload settings from the marker and reads `OUTPUT_DIR` and persistent state from the configured configstore.
7. When upload is disabled, no upload marker is created or consumed.
8. In-progress sentinel is created at run start and removed at completion and handled error exits.
9. `--upload-interval` is validated to require `--upload-enable`.

## Configstore keys

Persistent configstore keys include uptime, kernel version, meminsight version, report version, run iterations, run interval, run id, output format, output directory, backup settings, and fragmentation state. Upload-only keys are not written to the configstore.

## Upload marker keys

The temporary marker contains:

- `CONFIGSTORE_PATH`: absolute path to the selected output-directory configstore
- `RUN_ID`: run identity used by the uploader
- `UPLOAD_ENABLED`: `1` when upload is enabled
- `UPLOAD_INTERVAL`: requested upload cadence in seconds

## Source anchors

writeConfigStore(), touchFile(), removeFileIfPresent(), main(), collectSystemMemoryStats(), handleConfigMode() in src/meminsight.c
