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

## Source anchors

readSystemCpuStat(), saveSystemCpuStat(), saveSystemCpuStat_JSON(), main() in src/meminsight.c
