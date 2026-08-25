## 1. Header and Constants

- [x] 1.1 Add `PROC_STAT_FILE` path macro and `CSV_CPUSTAT_HEADER` section header macro to `src/meminsight.h`
- [x] 1.2 Define the 8 CPU field names as a constant string array or header format in `src/meminsight.h`

## 2. CSV Collection Function

- [x] 2.1 Implement `saveCpuStat(FILE *out)` in `src/meminsight.c` that opens `/proc/stat`, reads the first `cpu` line, parses 8 unsigned long fields (user, nice, system, idle, iowait, irq, softirq, steal), and writes the CSV section (header row + value row)
- [x] 2.2 Handle partial parse (fewer than 8 fields) by zero-filling missing fields and logging a warning
- [x] 2.3 Handle file open failure by logging an error and returning without crashing

## 3. JSON Collection Function

- [x] 3.1 Implement `saveCpuStat_JSON(void *root)` that reads `/proc/stat`, parses the aggregate CPU line, and adds a `"cpustat"` JSON object with 8 numeric fields to the root object
- [x] 3.2 Handle graceful fallback (skip cpustat object) when `/proc/stat` is unreadable

## 4. Integration into Collection Loop

- [x] 4.1 Call `saveCpuStat(output)` in `collectSystemMemoryStats()` after `saveMeminfo()` and before `saveFragmentationInfo()`
- [x] 4.2 Call `saveCpuStat_JSON(root)` in the JSON output path after `saveMeminfo_JSON()` and before fragmentation JSON
- [x] 4.3 Call `saveCpuStat()` / `saveCpuStat_JSON()` in `handleConfigMode()` path if meminfo is emitted there

## 5. Test Fixtures

- [x] 5.1 Create fixture directory `test/10-proc-stat-cpu/` with a sample `/proc/stat` file containing a realistic aggregate `cpu` line
- [x] 5.2 Create expected CSV output file for the fixture (test/10-cpu-stat-sample/ used by test runner; 10-proc-stat-cpu/ available for enhanced testing)
- [x] 5.3 Verify fixture passes via `run_ut.sh` (TESTME mode) - Test 15 validates CPU stat raw section

## 6. T2 Format Support

- [x] 6.1 Implement CPU state tracking in `writeT2Report()` for delta and percentage computation
- [x] 6.2 Emit T2 format fields: `cpu_stats.user`, `cpu_stats.system`, `cpu_stats.idle`, `cpu_stats.iowait`
- [x] 6.3 Compute and emit T2 deltas: `cpu_stats.delta_user`, `cpu_stats.delta_system`, `cpu_stats.delta_idle`, `cpu_stats.delta_total`
- [x] 6.4 Compute and emit T2 CPU percentage: `cpu_stats.cpu_percent = (delta_user + delta_system) / delta_total * 100.0`
- [x] 6.5 Validate T2 format output with fixture tests (--fmt t2 with CPU stat fixture)

## 7. Documentation

- [x] 7.1 Update design.md with D6 decision on T2 format state tracking and delta computation
- [x] 7.2 Update C13 spec (14-system-cpu-stat-raw-collection.md) to document T2 format behavior
- [x] 7.3 Update proposal.md to clarify 10-field implementation and T2 format support
- [x] 7.4 Update `README.md` output section to mention CPU stats in report (Already completed: Features, Test Mode, Compatibility Notes sections updated)
- [x] 7.5 Update `CHANGELOG.md` with the new capability entry (Already completed in version 2.0.0: "Add CPU stats and JSON bandwidth reporting")
