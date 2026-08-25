## Why

Meminsight currently collects per-process CPU time (utime + stime from `/proc/<pid>/stat`) but lacks system-wide CPU utilization data. The aggregate CPU line in `/proc/stat` provides a breakdown of how CPU time is distributed across user, nice, system, idle, iowait, irq, softirq, and steal — essential for correlating memory pressure with CPU load on RDK devices. Adding this enables operators to detect CPU saturation alongside memory exhaustion in a single lightweight tool.

## What Changes

- Add a new always-on collection step that reads the first (aggregate) `cpu` line from `/proc/stat` each iteration.
- Parse 10 fields: user, nice, system, idle, iowait, irq, softirq, steal, guest, guest_nice (in clock ticks). Older kernels with fewer fields are zero-filled.
- Emit a `CPUStat:` CSV section (header row + value row) following the same pattern as the existing `Meminfo:` section.
- Emit equivalent JSON fields under a `"cpu_stat"` object when JSON output is enabled.
- For T2 format (`--fmt t2`): maintain state between iterations to compute CPU deltas and CPU utilization percentage.
- Collection is always-on (no opt-in flag required), consistent with meminfo behavior.

## Capabilities

### New Capabilities
- `proc-stat-cpu-collection`: System-wide CPU stats extraction from the aggregate `cpu` line of `/proc/stat`, with CSV and optional JSON serialization.

### Modified Capabilities
- `09-csv-report-schema`: Add a new `CPUStat:` section to the CSV report layout between meminfo and fragmentation sections.
- `10-json-output`: Add `cpu_stat` object to JSON output containing the same 10 CPU fields.

## Impact

- **Code**: New `readSystemCpuStat()`, `saveSystemCpuStat()`, and `saveSystemCpuStat_JSON()` functions in `src/meminsight.c`; called unconditionally in the main collection loop. T2 format uses existing `writeT2Report()` with added CPU state tracking.
- **Header**: New field definitions and CSV section header macros in `src/meminsight.h`. New `CpuStatSnapshot` struct for internal state.
- **Tests**: New fixture directories (e.g., `test/10-cpu-stat-sample/`, `test/11-cpu-stat-legacy-fields/`) with sample `/proc/stat` content and expected output.
- **Report schema**: CSV reports gain a new `CPUStat:` section; JSON adds `cpu_stat` object; T2 adds `cpu_stats.*` and delta/percentage fields. Existing downstream parsers using section headers unaffected.
- **Dependencies**: None — `/proc/stat` is universally available on Linux.
