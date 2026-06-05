## Why

Meminsight currently collects per-process CPU time (utime + stime from `/proc/<pid>/stat`) but lacks system-wide CPU utilization data. The aggregate CPU line in `/proc/stat` provides a breakdown of how CPU time is distributed across user, nice, system, idle, iowait, irq, softirq, and steal — essential for correlating memory pressure with CPU load on RDK devices. Adding this enables operators to detect CPU saturation alongside memory exhaustion in a single lightweight tool.

## What Changes

- Add a new always-on collection step that reads the first (aggregate) `cpu` line from `/proc/stat` each iteration.
- Parse 8 fields: user, nice, system, idle, iowait, irq, softirq, steal (in clock ticks).
- Emit a `/proc/stat:` CSV section (header row + value row) following the same pattern as the existing `/proc/meminfo:` section.
- Emit equivalent JSON fields under a `"cpustat"` object when JSON output is enabled.
- Collection is always-on (no opt-in flag required), consistent with meminfo behavior.

## Capabilities

### New Capabilities
- `proc-stat-cpu-collection`: System-wide CPU stats extraction from the aggregate `cpu` line of `/proc/stat`, with CSV and optional JSON serialization.

### Modified Capabilities
- `09-csv-report-schema`: Add a new `/proc/stat:` section to the CSV report layout between meminfo and fragmentation sections.
- `10-json-output`: Add `cpustat` object to JSON output containing the same 8 CPU fields.

## Impact

- **Code**: New `saveCpuStat()` and `saveCpuStat_JSON()` functions in `src/meminsight.c`; called unconditionally in the main collection loop.
- **Header**: New field definitions and CSV section header macro in `src/meminsight.h`.
- **Tests**: New fixture directory (e.g., `test/10-proc-stat-cpu/`) with sample `/proc/stat` content and expected output.
- **Report schema**: CSV reports gain a new section; existing downstream parsers must tolerate the additional section.
- **Dependencies**: None — `/proc/stat` is universally available on Linux.
