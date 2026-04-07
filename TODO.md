# MemInsight TODO

## Priority Tasks
- [x] Integrate cJSON with runtime dynamic linking for JSON support.
- [x] Add CLI/config flags for JSON output format selection and `--json-pretty`.
- [x] Add fragmentation data collection flow:
  - Check `/proc/pagetypeinfo` first; if present, collect and dump to CSV/JSON.
  - Fallback to `/proc/buddyinfo`; if present, collect and dump to CSV/JSON.
  - If neither exists, don't log any data.
- [x] Change default output directory to `/opt/meminsight` (keep `/tmp/meminsight` as configurable fallback).
- [ ] Add upload cadence logic to trigger upload through external script/binary based on configured intervals.
- [x] Document meminsight execution flow for both manual and automatic/systemd runs.
- [ ] Reorganize code and introduce centralized macros/constants for modular configuration.
- [x] Add `agents/`, `skills/`, `instructions/`, and `copilot-instructions.md` in `.github/`.
- [ ] Add cleanup, upload, and backup cadence policy for existing reports.
- [x] Add sample systemd service and sample `.bb` recipe for build/integration.
- [x] Create organized `scripts/` directory with modular build and detection tools.
  - [x] `scripts/build_memleak.sh` - Clone and build memleakutil library
  - [x] `scripts/detect_leak.sh` - Run meminsight with leak detection instrumentation
  - [x] `scripts/README.md` - Complete documentation of script organization
- [x] Perform comprehensive code audit and generate hardening recommendations.
  - [x] Created `docs/CODE_AUDIT_AND_HARDENING.md` with findings
  - [x] Identified 3 CRITICAL resource leaks and mitigation strategies
  - [x] Identified 7 high/medium issues with remediation plans
  - [x] Identified 5 low-severity optimization opportunities
- [x] Integrate memleakutil into CI/CD workflow (`native_full_build.yml`).
  - [x] Updated workflow to use new `scripts/build_memleak.sh`
  - [x] Updated workflow to use new `scripts/detect_leak.sh`
  - [x] Added full leak detection report output to workflow summary

## Priority Tasks - Phase 2 (Code Hardening)
- [ ] Apply CRITICAL fixes from CODE_AUDIT_AND_HARDENING.md:
  - [ ] Fix file handle leak in `parseConfig()` - ensure fclose on all paths
  - [ ] Fix directory handle leak in `getPIDByProcessName()` - close before break
  - [ ] Add null check in cJSON cleanup path before `Delete()` call
- [ ] Apply HIGH fixes:
  - [ ] Add bounds check for PSS accumulation (prevent integer overflow)
  - [ ] Add division-by-zero guard in bandwidth calculations
  - [ ] Add bounds on directory iteration (maxEntries limit)
- [ ] Extract fragmentation parsing into separate module (`src/frag_parsers.c/h`)
- [ ] Create safe string utilities module (`src/str_utils.c/h`)
- [ ] Create file operations utilities module (`src/file_utils.c/h`)

## Optional Tasks
- [ ] Add auto component release workflow with Slack approval gate.
- [ ] Develop meminsight-related MCP tools for actions such as release, test run, native/container build.
- [x] Use memleakutils in `native_full_build.yml` for leak identification. (COMPLETE)
- [ ] Update `README.md` and `crashupload.wiki` documentation with new script organization.
- [ ] Improve test organization, generation, and coverage reporting.
- [ ] Add static analysis tools (clang-tidy, cppcheck) to CI/CD pipeline.
- [ ] Generate coverage reports and upload to codecov.

## Missing But Recommended
- [ ] Define performance budgets and acceptance thresholds (CPU/memory overhead, sample latency, upload latency).
- [ ] Add parser compatibility fixtures for multiple kernel variants (`pagetypeinfo`/`buddyinfo` format drift).
- [ ] Add security hardening tasks for uploader path (TLS validation, token/cert handling, secret redaction in logs).
- [ ] Add offline buffering and retry policy details (queue limits, backoff, drop policy, corruption handling).
- [ ] Add report schema versioning and migration plan (CSV/JSON fields, backward compatibility contract).
- [ ] Add observability counters/health metrics (samples collected, parser failures, upload failures, queue depth).
- [ ] Add rollback/feature-flag plan for staged deployment on mixed RDK device classes.
