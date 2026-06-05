## 1. Header and Constants

- [ ] 1.1 Add `PROC_STAT_FILE` path macro and `CSV_CPUSTAT_HEADER` section header macro to `src/meminsight.h`
- [ ] 1.2 Define the 8 CPU field names as a constant string array or header format in `src/meminsight.h`

## 2. CSV Collection Function

- [ ] 2.1 Implement `saveCpuStat(FILE *out)` in `src/meminsight.c` that opens `/proc/stat`, reads the first `cpu` line, parses 8 unsigned long fields (user, nice, system, idle, iowait, irq, softirq, steal), and writes the CSV section (header row + value row)
- [ ] 2.2 Handle partial parse (fewer than 8 fields) by zero-filling missing fields and logging a warning
- [ ] 2.3 Handle file open failure by logging an error and returning without crashing

## 3. JSON Collection Function

- [ ] 3.1 Implement `saveCpuStat_JSON(void *root)` that reads `/proc/stat`, parses the aggregate CPU line, and adds a `"cpustat"` JSON object with 8 numeric fields to the root object
- [ ] 3.2 Handle graceful fallback (skip cpustat object) when `/proc/stat` is unreadable

## 4. Integration into Collection Loop

- [ ] 4.1 Call `saveCpuStat(output)` in `collectSystemMemoryStats()` after `saveMeminfo()` and before `saveFragmentationInfo()`
- [ ] 4.2 Call `saveCpuStat_JSON(root)` in the JSON output path after `saveMeminfo_JSON()` and before fragmentation JSON
- [ ] 4.3 Call `saveCpuStat()` / `saveCpuStat_JSON()` in `handleConfigMode()` path if meminfo is emitted there

## 5. Test Fixtures

- [ ] 5.1 Create fixture directory `test/10-proc-stat-cpu/` with a sample `/proc/stat` file containing a realistic aggregate `cpu` line
- [ ] 5.2 Create expected CSV output file for the fixture
- [ ] 5.3 Verify fixture passes via `run_ut.sh` (TESTME mode)

## 6. Documentation

- [ ] 6.1 Update `README.md` output section to mention CPU stats in report
- [ ] 6.2 Update `CHANGELOG.md` with the new capability entry
