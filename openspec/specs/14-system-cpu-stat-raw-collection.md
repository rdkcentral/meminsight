# C13 - System-wide CPU counter collection from /proc/stat

## Scope

This specification defines collection and serialization of system-wide aggregate CPU counters from `/proc/stat`.

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

1. meminsight reads the aggregate `cpu` line on each collection iteration and parses up to 10 fields in kernel order.
2. Missing trailing fields are zero-filled; malformed or unavailable input is handled fail-soft without terminating capture.
3. CPU counters are parsed and stored as `uint64_t` values to avoid overflow on 32-bit targets and long-uptime systems.
4. Under TESTME, an optional stat fixture path can replace `/proc/stat` for deterministic validation.

## CSV output

CSV output appends a `CPUStat` section after the meminfo section and before optional fragmentation output. The header order is:

```text
user,nice,system,idle,iowait,irq,softirq,steal,guest,guest_nice
```

The value row contains the corresponding raw cumulative tick counters.

## JSON output

When cJSON support is available, JSON output includes a `cpu_stat` object containing the same 10 raw counters. If cJSON is unavailable, the existing CSV fallback remains in effect.

## T2 output

For `--fmt t2`, the report includes selected raw values under `cpu_stats.user`, `cpu_stats.system`, `cpu_stats.idle`, and `cpu_stats.iowait`. Module-level state stores the previous 64-bit counters and emits:

- `cpu_stats.delta_user`
- `cpu_stats.delta_system`
- `cpu_stats.delta_idle`
- `cpu_stats.delta_total`
- `cpu_stats.cpu_percent`

The first iteration emits zero deltas. Subsequent iterations calculate deltas from the previous sample. `cpu_percent` is `(delta_user + delta_system) / delta_total * 100.0` when `delta_total` is greater than zero; otherwise it is zero.

## Source anchors

readSystemCpuStat(), saveSystemCpuStat(), saveSystemCpuStat_JSON(), writeT2Report() in src/meminsight.c
