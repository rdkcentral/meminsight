# C06 - Meminfo section extraction and serialization (As-Is)

## Scope

This specification defines how meminsight extracts a bounded set of fields from /proc/meminfo and writes them into reports.

## Field set

The current implementation targets these fields:

- MemTotal
- MemFree
- MemAvailable
- Buffers
- Cached
- SwapCached
- Active(anon)
- Inactive(anon)
- Active(file)
- Inactive(file)
- SwapTotal
- SwapFree
- AnonPages
- Mapped
- Shmem
- Slab
- KernelStack
- VmallocUsed
- CmaFree
- CmaTotal

## Current behavior

1. CSV output writes a meminfo section header and two rows: field names and values.
2. The parser learns skip offsets and header order on first successful parse and reuses them for subsequent iterations.
3. JSON output writes the same field set as numeric key/value entries under a meminfo object.
4. Missing or parse-failed lines are skipped without process termination.

## Source anchors

saveMeminfo(), saveMeminfo_JSON() in src/meminsight.c
