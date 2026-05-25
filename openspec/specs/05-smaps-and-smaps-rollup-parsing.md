# C05 - Memory map parsing from smaps and smaps_rollup (As-Is)

## Scope

This specification defines how meminsight parses process memory map data from /proc/<pid>/smaps or /proc/<pid>/smaps_rollup.

## Parser source selection

1. When forced by --smaps, source is smaps.
2. Otherwise, smaps_rollup is selected when available.
3. If smaps_rollup is unavailable, source is smaps.

## Parsed fields

The current parser accumulates:

- Rss
- Pss
- Shared_Clean
- Private_Clean
- Private_Dirty
- SwapPss (when present)

## Current implementation strategy

1. The parser performs initial line-offset discovery for relevant fields.
2. Learned offsets are reused in subsequent parsing calls for reduced scanning overhead.
3. If SwapPss is absent in an observed source layout, parsing continues without failing the process record.
4. Shared_Clean handling differs between smaps and smaps_rollup code paths according to current implementation logic.

## Failure handling

1. If a process map file cannot be opened or parsed, that process entry is skipped.
2. Process collection continues for remaining PIDs.

## Source anchors

getProcessInfos(), getProcessInfos_initial(), getProcessInfos_learnt(), getProcessInfos_rollup(), getProcessInfos_rollup_learnt() in src/meminsight.c
