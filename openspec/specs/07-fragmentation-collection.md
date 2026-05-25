# C07 - Fragmentation collection with pagetypeinfo preference and buddyinfo fallback (As-Is)

## Scope

This specification defines optional memory-fragmentation collection behavior enabled by `--frag`.

## Source precedence

1. Preferred source: /proc/pagetypeinfo.
2. Fallback source: /proc/buddyinfo.
3. If neither source is available, a source-unavailable status is emitted.

## Enablement

1. Fragmentation collection is disabled unless `--frag` is passed.
2. Source selection is performed once at startup and reused for all iterations in the run.

## CSV behavior

1. When pagetypeinfo is selected and parse succeeds, rows include node, zone, type, and per-order values.
2. When buddyinfo is selected and parse succeeds, rows include node, zone, and per-order values.
3. Parse failures emit explicit parse status lines for the selected source.

## JSON behavior

1. Fragmentation data is emitted as a structured object with source metadata.
2. Pagetypeinfo includes page_block_order and pages_per_block when parsed.
3. Parse failures emit explicit parse_status markers.

## Source anchors

selectFragmentationSource(), saveFragmentationInfo(), writePagetypeInfoCSV(), writeBuddyinfoCSV(), saveFragmentationInfo_JSON(), addPagetypeInfoJSON(), addBuddyinfoJSON() in src/meminsight.c
