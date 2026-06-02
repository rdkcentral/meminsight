# C09 - CSV report schema and section emission (As-Is)

## Scope

This specification defines CSV output structure and ordering.

## File naming

Current filename format:

`<mac>_<timestamp>_iter<iteration>_meminsight.csv`

## Current section order

1. Metadata header row.
2. Metadata value row.
3. Meminfo section.
4. Optional fragmentation section (only when `--frag`).
5. Processes section header and per-process rows sorted by pss descending.
6. Synthetic total row with pid 0 and name Total.
7. Optional bandwidth section when available.

## Metadata fields

Current metadata fields are:

- FIRMWARE_NAME
- MAC_ADDRESS
- TIMESTAMP
- UPTIME
- KERNEL_VERSION
- REPORT_VERSION
- ITERATION
- RUN_ITERATIONS
- RUN_INTERVAL
- RUN_ID

## Source anchors

collectSystemMemoryStats(), handleConfigMode(), writeProcessInfo(), saveMeminfo(), saveFragmentationInfo(), collectBandwidthData() in src/meminsight.c
