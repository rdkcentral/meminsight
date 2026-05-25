# C13 - Capability parity verification matrix (As-Is)

## Scope

This document records the current implementation-to-spec parity verification for the As-Is baseline.

## Verification timestamp

- Date: 2026-05-25
- Verification basis: source scan of src/meminsight.c and existing openspec/specs capability files.

## Parity matrix

| Capability ID | Spec file | Implementation anchors |
|---|---|---|
| C01 | 01-cli-and-execution-modes.md | main(), printHelpAndUsage() |
| C02 | 02-output-directory-and-run-lifecycle.md | ensure_output_dir(), clear_dir_contents(), touchFile(), removeFileIfPresent(), initializeSetupInfo() |
| C03 | 03-config-mode-and-whitelist-capture.md | parseConfig(), getPIDByProcessName(), handleConfigMode() |
| C04 | 04-process-discovery-and-filtering.md | collectSystemMemoryStats(), fillProcessStatFields(), addULSaturating(), addProcessInfo(), writeProcessInfo() |
| C05 | 05-smaps-and-smaps-rollup-parsing.md | getProcessInfos(), getProcessInfos_initial(), getProcessInfos_learnt(), getProcessInfos_rollup(), getProcessInfos_rollup_learnt() |
| C06 | 06-meminfo-collection.md | saveMeminfo(), saveMeminfo_JSON() |
| C07 | 07-fragmentation-collection.md | selectFragmentationSource(), saveFragmentationInfo(), writePagetypeInfoCSV(), writeBuddyinfoCSV(), saveFragmentationInfo_JSON() |
| C08 | 08-ddr-bandwidth-collection.md | updateBandwidthAvailability(), collectBandwidthData() |
| C09 | 09-csv-report-output.md | collectSystemMemoryStats(), handleConfigMode(), writeProcessInfo(), saveMeminfo(), saveFragmentationInfo(), collectBandwidthData() |
| C10 | 10-json-report-output-optional.md | loadCjson(), unloadCjson(), saveMeminfo_JSON(), saveFragmentationInfo_JSON(), writeProcessInfo_JSON(), writeJSONToFile(), main() |
| C11 | 11-upload-and-state-files.md | writeConfigStore(), touchFile(), removeFileIfPresent(), main(), collectSystemMemoryStats(), handleConfigMode() |
| C12 | 12-testme-fixture-mode.md | main(), testGetProcessInfos_Parse(), testGetProcessInfos(), getProcessInfos(), saveMeminfo(), selectFragmentationSource(), collectSystemMemoryStats() |

## Verification result

- All capabilities in the current As-Is catalog have corresponding spec coverage.
- No uncovered core capability was found during this verification pass.
- The capability index and this matrix are consistent for baseline onboarding.
