# C13 - System-wide CPU raw counter collection from /proc/stat (As-Is)

## Scope

This specification defines collection of system-wide aggregate CPU counters from `/proc/stat`.

## Source and field order

meminsight reads the aggregate `cpu` line and captures raw values in Linux kernel-defined order:

- user
- nice
- system
- idle
- iowait
- irq
- softirq
- steal
- guest
- guest_nice

## Current behavior

1. meminsight reports raw tick counters as-is without deriving percentages.
2. CSV output appends a `CPUStat` section with header order matching the kernel field order.
3. JSON output includes a `cpu_stat` object with the same 10 fields.
4. Parse/source failures are fail-soft and CPU stat section/object emission is skipped, without terminating capture.
5. Under TESTME, an optional stat fixture path can be provided to inject deterministic CPU data.
6. CPU counters are parsed and stored as 64-bit values to avoid rollover on long-uptime 32-bit environments.

## T2 format behavior

When `--fmt t2` is selected:
1. T2 format maintains state between iterations to compute CPU deltas and derived metrics.
2. Individual fields emitted as `cpu_stats.user`, `cpu_stats.system`, `cpu_stats.idle`, `cpu_stats.iowait` (select 4 fields for brevity).
3. Deltas computed from prior iteration: `cpu_stats.delta_user`, `cpu_stats.delta_system`, `cpu_stats.delta_idle`, `cpu_stats.delta_total`.
4. Derived metric: `cpu_stats.cpu_percent = (delta_user + delta_system) / delta_total * 100.0` (when delta_total > 0; otherwise 0).
5. First iteration has no prior state, so all deltas are emitted as `0`.
6. State is module-level static and persists across iterations within a single run.

## Source anchors

readSystemCpuStat(), saveSystemCpuStat(), saveSystemCpuStat_JSON(), writeT2Report() in src/meminsight.c
