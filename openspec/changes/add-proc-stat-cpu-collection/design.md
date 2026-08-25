## Context

Meminsight collects per-iteration memory snapshots from Linux procfs. The existing collection pipeline reads `/proc/meminfo` (system memory), `/proc/<pid>/stat` + `/proc/<pid>/smaps_rollup` (per-process), and optional fragmentation/bandwidth sources. Per-process CPU time (utime + stime) is already captured from `/proc/<pid>/stat`, but there is no system-wide CPU utilization breakdown.

The aggregate `cpu` line in `/proc/stat` provides cumulative clock ticks for 8 categories (user, nice, system, idle, iowait, irq, softirq, steal). This data is universally available on Linux and requires no additional dependencies.

The implementation must follow meminsight's defensive, embedded-safe patterns: fail soft on parse errors, avoid dynamic allocation where possible, and maintain CSV backward compatibility by appending (not reordering) sections.

## Goals / Non-Goals

**Goals:**
- Extract the 8 aggregate CPU fields from the first `cpu` line of `/proc/stat` each iteration
- Emit a new `/proc/stat:` CSV section between meminfo and fragmentation
- Emit a `cpustat` JSON object when JSON output is enabled
- Follow the same learned-offset optimization pattern used by `saveMeminfo()`
- Provide fixture-based test coverage

**Non-Goals:**
- Per-core CPU breakdown (cpu0, cpu1, etc.) — not in scope
- CPU percentage calculation or delta computation between iterations — raw ticks only
- Additional `/proc/stat` counters (ctxt, processes, procs_running) — future enhancement
- Configurable field selection — all 8 fields always collected

## Decisions

### D1: Parse only the first `cpu` aggregate line

**Rationale**: The first line (`cpu  ...`) contains the system-wide total. Per-core lines would multiply output size by core count and add complexity with no clear telemetry requirement today. Consistent with the user's stated scope.

**Alternative considered**: Parsing per-core lines. Rejected because RDK devices vary in core count (1-8), output size would be unpredictable, and downstream parsers would need to handle variable-width sections.

### D2: Always-on collection (no opt-in flag)

**Rationale**: `/proc/stat` is universally available on Linux and the parse cost is negligible (single line, 8 integers). Mirrors meminfo collection which is also always-on. Avoids CLI complexity.

**Alternative considered**: `--cpu` flag similar to `--frag`. Rejected because there's no platform where `/proc/stat` is unavailable or expensive.

### D3: CSV section placement — after meminfo, before fragmentation

**Rationale**: Logically groups system-wide metrics (meminfo, cpustat) before per-process data. The fragmentation section is optional; placing cpustat before it keeps always-on sections contiguous.

**Alternative considered**: After fragmentation. Rejected for logical grouping reasons.

### D4: Field values as raw clock ticks (unsigned long)

**Rationale**: Raw values preserve full fidelity and allow downstream tools to compute deltas and percentages. Matches how meminfo reports raw kB values. No loss of information.

**Alternative considered**: Computing percentages within meminsight. Rejected because percentages require delta between two samples (introducing state) and the tool's design is snapshot-based.

### D5: Reuse learned-offset pattern from saveMeminfo()

**Rationale**: Although `/proc/stat`'s `cpu` line is simpler than meminfo (space-separated values on a single known line), using a consistent parsing approach keeps the codebase uniform. For this case, the "learning" is simply verifying the line starts with `cpu ` and sscanf-parsing 8 unsigned longs.

**Alternative considered**: Direct sscanf without the learned-offset wrapper. Acceptable alternative; the implementation may simplify to a direct parse since there's only one target line.

### D6: T2 format includes delta computation and CPU percentage

**Rationale**: The T2 telemetry format (`--fmt t2`) maintains state between iterations to compute deltas for CPU user/system/idle/total time and derives a CPU percentage metric. This enables T2 consumers to detect CPU saturation trends without requiring client-side delta computation.

CSV and JSON formats emit raw tick values only; T2 format augments this with:
- `cpu_stats.delta_user`, `cpu_stats.delta_system`, `cpu_stats.delta_idle`, `cpu_stats.delta_total` (deltas from previous iteration)
- `cpu_stats.cpu_percent` (derived: (delta_user + delta_system) / delta_total * 100.0)

**Implementation detail**: T2 uses a module-level static array `prevCpu[8]` to track the prior iteration's values. The first iteration has no delta (all zeros); subsequent iterations compute deltas and derive percentages.

**Alternative considered**: Emit raw values in all formats uniformly. Rejected because T2 format already maintains state for meminfo and process deltas; CPU state tracking is consistent with existing T2 patterns.

## Risks / Trade-offs

- **[Risk] Kernel versions with fewer CPU fields** → Mitigation: If sscanf parses fewer than 8 fields (e.g., older kernels without `steal`), zero-fill the missing fields and log a warning. The parser tolerates partial data.
- **[Risk] CSV section order change breaks downstream parsers** → Mitigation: New section is appended between existing sections; parsers that use section headers (e.g., grep for `/proc/meminfo:`) are unaffected. Document the new section in release notes.
- **[Trade-off] Always-on adds a small amount of output per iteration** → Acceptable: 2 CSV lines (~100 bytes) per iteration is negligible for embedded targets.
