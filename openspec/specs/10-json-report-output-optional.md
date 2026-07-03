# C10 - Optional JSON report output with runtime cJSON loading and CSV fallback (As-Is)

## Scope

This specification defines JSON report behavior when built with optional cJSON support.

## Enablement

1. JSON output is selected by `--fmt json`.
2. `--json-pretty` is valid only with `--fmt json`.
3. If build does not include cJSON support, JSON request falls back to CSV.

## Runtime library loading

1. cJSON symbols are loaded at runtime using dynamic loading.
2. If runtime library loading fails, output format falls back to CSV.
3. Symbol-resolution failures also trigger CSV fallback.

## JSON structure

Current JSON report includes:

- top-level run metadata fields
- meminfo object
- cpu_stat object (raw aggregate `/proc/stat` fields)
- optional fragmentation object when --frag
- processes array with per-process rows
- synthetic total row appended in processes array

## Top-level metadata fields

Top-level JSON metadata includes all CSV metadata fields, including:

- FIRMWARE_NAME
- MAC_ADDRESS
- TIMESTAMP
- UPTIME
- KERNEL_VERSION
- REPORT_VERSION
- ITERATION
- RUN_ITERATIONS
- RUN_INTERVAL
- RUN_ID

## Serialization behavior

1. Pretty or compact serialization is selected by `--json-pretty`.
2. JSON root object is released after each write.

## Source anchors

loadCjson(), unloadCjson(), saveMeminfo_JSON(), saveSystemCpuStat_JSON(), saveFragmentationInfo_JSON(), writeProcessInfo_JSON(), writeJSONToFile(), main() in src/meminsight.c
