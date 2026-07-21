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
4. CPUStat section (raw aggregate `/proc/stat` counters) when source is available.
5. Optional fragmentation section (only when `--frag`).
6. Optional bandwidth section when available.
7. Processes section header and per-process rows sorted by pss descending.
8. Synthetic total row with pid 0 and name Total.

## CPUStat fields

Current CPUStat fields are emitted in this exact order:

- USER
- NICE
- SYSTEM
- IDLE
- IOWAIT
- IRQ
- SOFTIRQ
- STEAL
- GUEST
- GUEST_NICE

When `/proc/stat` cannot be read or parsed, CPUStat section emission is skipped and report generation continues.

## Schema version impact

CPUStat addition is a report-schema change and requires a report-version increment.

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

collectSystemMemoryStats(), handleConfigMode(), writeProcessInfo(), saveMeminfo(), saveSystemCpuStat(), saveFragmentationInfo(), collectBandwidthData() in src/meminsight.c
