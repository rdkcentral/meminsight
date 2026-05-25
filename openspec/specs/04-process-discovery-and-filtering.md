# C04 - Process discovery, stat enrichment, and kernel-thread filtering (As-Is)

## Scope

This specification defines system-wide process discovery and the enrichment/filtering applied before memory-map parsing.

## Current behavior

1. Process discovery iterates entries in /proc.
2. Entries are considered process candidates when the name parses to a positive PID.
3. Per-process base fields are read from `/proc/<pid>/stat`:
	- process name
	- minor faults
	- major faults
	- user+system CPU time
	- process flags
4. Kernel-thread inclusion is controlled by -a or --all.
5. When kernel-thread inclusion is disabled, entries with PF_KTHREAD are skipped.
6. On stat read failure for a process, that process is skipped and collection continues.

## Aggregation behavior

1. Per-process totals are accumulated for rss, pss, shared_clean, private_clean, private_dirty, and swap_pss.
2. Saturating arithmetic is used for totals to prevent unsigned overflow.
3. Process rows are sorted by descending pss before writing.

## Source anchors

collectSystemMemoryStats(), fillProcessStatFields(), addULSaturating(), addProcessInfo(), writeProcessInfo() in src/meminsight.c
