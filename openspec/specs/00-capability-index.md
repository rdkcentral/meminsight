# meminsight capability index (As-Is baseline)

This index defines the current capability catalog for the meminsight codebase and maps each capability to one specification file.

## Coverage rule

Every listed capability has a corresponding specification entry in this directory.

## Capability map

- C01: Command-line interface and execution mode resolution -> 01-cli-and-execution-modes.md
- C02: Output directory initialization and per-run file lifecycle -> 02-output-directory-and-run-lifecycle.md
- C03: Config file parsing and whitelist-based capture mode -> 03-config-mode-and-whitelist-capture.md
- C04: Process discovery, stat enrichment, and kernel-thread filtering -> 04-process-discovery-and-filtering.md
- C05: Memory map parsing from smaps and smaps_rollup -> 05-smaps-and-smaps-rollup-parsing.md
- C06: Meminfo section extraction and serialization -> 06-meminfo-collection.md
- C07: Fragmentation collection with pagetypeinfo preference and buddyinfo fallback -> 07-fragmentation-collection.md
- C08: DDR bandwidth section collection -> 08-ddr-bandwidth-collection.md
- C09: CSV report schema and section emission -> 09-csv-report-output.md
- C10: Optional JSON report output with runtime cJSON loading and CSV fallback -> 10-json-report-output-optional.md
- C11: Upload marker and configstore state file behavior -> 11-upload-and-state-files.md
- C12: TESTME fixture mode and parser regression checks -> 12-testme-fixture-mode.md

## Canonical implementation references

Primary implementation source: src/meminsight.c
Interface and constants source: src/meminsight.h

## Verification artifact

- Implementation parity matrix: 13-implementation-parity-matrix.md

## Architecture context artifact

- System-level baseline architecture: ../architecture/00-baseline-architecture.md
