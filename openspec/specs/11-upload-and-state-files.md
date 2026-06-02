# C11 - Upload marker and configstore state file behavior (As-Is)

## Scope

This specification defines the current upload signaling and run-state file behavior.

## Files

- /tmp/.meminsight_upload
- /tmp/.meminsight_configstore
- /tmp/.meminsight_inprogress

## Current behavior

1. When --upload-enable is supplied, an upload marker file is touched before capture flow.
2. At run startup, configstore is written or updated with run and environment metadata.
3. If configstore already contains the same key/value set, file rewrite is skipped.
4. In-progress sentinel is created at run start and removed at completion and handled error exits.
5. upload-interval value is validated to require upload-enable.

## Configstore keys

Current configstore keys include uptime, kernel version, meminsight version, report version, run iterations, run interval, run id, output format, upload enabled flag, upload interval, and output directory.

## Source anchors

writeConfigStore(), touchFile(), removeFileIfPresent(), main(), collectSystemMemoryStats(), handleConfigMode() in src/meminsight.c
