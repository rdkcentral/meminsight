# MemInsight TODO

> Priorities and phase structure are aligned to [SRS_meminsight_rdk_fragmentation.md](SRS_meminsight_rdk_fragmentation.md).
> SRS requirement IDs are shown in brackets where applicable.

---

## Phase 1 — Collector Core (FR-001 to FR-032) ✅ Mostly Complete

- [x] Integrate cJSON with runtime dynamic linking for JSON support. `[FR-022]`
- [x] Add CLI/config flags for JSON output format selection and `--json-pretty`. `[FR-021, FR-023]`
- [x] Add fragmentation data collection flow: `[FR-001, FR-003, FR-005]`
  - [x] Check `/proc/pagetypeinfo` first; if present, collect and dump to CSV/JSON.
  - [x] Fallback to `/proc/buddyinfo`; if present, collect and dump to CSV/JSON.
  - [x] If neither exists, skip fragmentation section without crash.
- [x] Change default output directory to `/opt/meminsight` (keep `/tmp/meminsight` as configurable fallback). `[FR-050, FR-051]`
- [x] Report metadata includes timestamp, uptime, device ID, firmware, kernel version, report version. `[FR-024]`
- [x] Prefer `smaps_rollup`; fallback to `smaps`; respect `-s` flag. `[FR-006]`
- [x] Probe for Amlogic DDR bandwidth file at runtime; collect if present, skip gracefully if absent. `[FR-030, FR-031, FR-032]`
- [x] JSON and CSV paths both emit collection results; no hard-link JSON dependency in CSV-only builds. `[FR-020, FR-021, FR-022]`
- [x] Add sample systemd service and sample `.bb` recipe for build/integration. `[OPS-001]`
- [ ] Make fragmentation source policy user-configurable via `--frag-source` flag. `[FR-004]`
  - Currently hardcoded to pagetypeinfo-preferred-with-buddyinfo-fallback (D-001 resolved).
  - Expose as a CLI/config option to support `buddyinfo-only` and `collect-both` modes.
- [ ] Document deterministic computation definition for every derived fragmentation metric in code comments. `[FR-011]`
- [ ] Centralise all CSV header strings into macros; remove any remaining inline hardcoded header strings. `[FR-025]`

---

## Phase 2 — Code Hardening (Code Audit & Review Fixes)

### Review Fixes — Applied 2026-04-07
- [x] Fix CLI `-o/--output` precedence: track explicit `cli_output_set` boolean; always prefer CLI when set regardless of value. *(config-mode output dir was wrongly skipped when user passed the default path)*
- [x] Fix config-mode process list writing: gate `writeProcessInfo()` to CSV path only; route JSON path through `writeProcessInfo_JSON()`. *(NULL FILE\* dereference when `--fmt json` active in config mode)* `[ERR-001]`
- [x] Fix JSON process-list memory leak on `CreateArray()` failure: call `freeProcessInfoList()` and abort iteration cleanly. `[ERR-001]`
- [x] Add `freeProcessInfoList()` cleanup helper to ensure the process linked list is freed on non-CSV and failure paths.

### CRITICAL Fixes Still Open (from CODE_AUDIT_AND_HARDENING.md)
- [ ] Fix file handle leak in `parseConfig()` — ensure `fclose()` on all error return paths.
- [ ] Fix directory handle leak in `getPIDByProcessName()` — `closedir()` before `break`.

### HIGH Fixes Still Open
- [ ] Add bounds check for PSS/RSS accumulation to prevent integer overflow on long-running captures.
- [ ] Add division-by-zero guard in bandwidth calculations.

### Code Modularity (LOW — carry forward if refactor scope widens)
- [ ] Reorganize code and introduce centralized macros/constants for modular configuration. `[FR-025]`
- [ ] Extract fragmentation parsing into separate module (`src/frag_parsers.c/h`).

---

## Phase 3 — Upload & Lifecycle Management `[FR-040 to FR-045]`

> SRS Feature: Scheduled Report Upload and Lifecycle Management — **Pending**

- [ ] Define and implement a separate uploader script/binary; MemInsight must not upload directly. `[FR-040, FR-041]`
- [ ] Implement upload trigger mechanism: MemInsight logs cadence plan; uploader polls or is signaled on schedule. `[FR-042]`
- [ ] Implement upload success/failure logging in uploader. `[FR-041]`
- [ ] Delete local report copies after confirmed successful upload. `[FR-041]`
- [ ] Implement retry-with-backoff policy in uploader when endpoint is unavailable. `[FR-044]`
- [ ] Ensure data collection continues uninterrupted when network/endpoint is unavailable. `[FR-043]`
- [ ] (SHOULD) Implement upload batching to reduce network overhead. `[FR-045]`

---

## Phase 4 — Storage, Retention & Pre-Capture Cleanup `[FR-050 to FR-054]`

> SRS Feature: Pre-capture cleanup — **Partial** (implemented in `scripts/detect_leak.sh` for CI; not in product path)

- [ ] Implement rotation/cleanup policy that runs on startup and by configurable retention cadence. `[FR-052]`
- [ ] Add pre-capture cleanup in the MemInsight product path (clear old reports before new capture). `[FR-052]`
- [ ] Implement archive compression (tar/gzip) before deletion. `[FR-053]`
- [ ] Add configurable retention limits: max age (hours), max size (MB), max file count. `[FR-054]`
- [ ] Expose retention config via CLI flags: `--retention-max-age`, `--retention-max-size`, `--retention-max-files`. `[FR-054, §11.1]`

---

## Phase 5 — Service, NTP & Runtime Dependencies `[FR-060, FR-061]`

- [ ] Declare NTP synchronization dependency in systemd service unit before regular reporting starts. `[FR-060]`
- [ ] Implement configurable NTP-unsynced startup behavior: `wait-with-timeout`, `run-with-unsynced-flag`, or `defer-upload`. `[FR-061]`
- [ ] Add network/NTP readiness ordering to service unit dependencies. `[OPS-001]`

---

## Phase 6 — Pre-Release QA, Baseline Comparison & OTA `[SRS Features]`

> SRS Feature: MemInsight execution during pre-release QA testing — **Partial**
> SRS Feature: Pre-Release Baseline Comparison — **Pending**
> SRS Feature: OTA Delivery of Memory Analysis Tooling — **Pending**

- [ ] Define and document relQA execution policy: when MemInsight must run, report artifact retention, and pipeline ingestion path.
- [ ] Implement anomaly identification and highlighting in CI pipeline output (memory regressions, high fragmentation events).
- [ ] Build baseline comparison tooling: capture baseline metrics per validated release; compare new build output and flag regressions.
- [ ] Validate OTA update flow: MemInsight package installs/upgrades cleanly over existing; profiling workflows remain functional post-OTA.

---

## Documentation & Release Automation `[FR-080 to FR-082]`

- [x] Document MemInsight execution flow for both manual and automatic/systemd runs. `[FR-080]`
- [x] Add `agents/`, `skills/`, `instructions/`, and `copilot-instructions.md` in `.github/`.
- [x] Create organized `scripts/` directory with modular build and detection tools. `[FR-080]`
  - [x] `scripts/build_memleak.sh` — Clone and build memleakutil library.
  - [x] `scripts/detect_leak.sh` — Run MemInsight with leak detection instrumentation.
  - [x] `scripts/run_ut.sh` — Fixture-based unit test runner.
  - [x] `scripts/README.md` — Script organization documentation.
- [x] Perform comprehensive code audit and generate hardening recommendations (`docs/CODE_AUDIT_AND_HARDENING.md`). `[FR-080]`
- [x] Integrate memleakutil into CI/CD workflow (`native_full_build.yml`). `[FR-082]`
- [ ] Update `README.md` with uploader architecture, new storage/retention flags, and script organization. `[FR-080]`
- [ ] Update `meminsight.wiki` with operator-level and developer-level guidance. `[FR-081]`
- [ ] Add auto-release workflow with PR-inferred semver bump, Slack approval gate, and graceful rejection handling. `[FR-082]`
- [ ] Update `docs/CODE_AUDIT_AND_HARDENING.md` status table to reflect fixes applied 2026-04-07.

---

## CI / Test Coverage `[TST-001 to TST-042]`

- [x] Fixture-based unit tests (TESTME) for meminfo and smaps parsing. `[TST-001]`
- [x] Tests 6 and 7: fragmentation fixture tests (pagetypeinfo and buddyinfo paths). `[TST-010, TST-011]`
- [x] Deterministic fixture-only behavior in TESTME mode (no host `/proc` interference). `[TST-001]`
- [x] Negative tests: duplicate meminfo field, duplicate smaps field.
- [ ] Add fixture coverage for `/proc/buddyinfo` format variants across kernel versions. `[TST-010]`
- [ ] Add fixture coverage for `/proc/pagetypeinfo` migration-type layout variants. `[TST-011]`
- [ ] Add fault-injection fixtures: missing buddyinfo, missing pagetypeinfo. `[TST-030, TST-031]`
- [ ] Add static analysis tooling (clang-tidy, cppcheck) to CI pipeline.
- [ ] Generate coverage reports and upload to codecov.
- [ ] Improve test organization and coverage reporting.

---

## Stretch Goals

- [ ] Develop MCP tools for MemInsight actions (release, test run, native/container build).
- [ ] AI-powered analysis of memory reports: identify patterns, anomalies, and regressions; summarize in human-readable insights. *(SRS Stretch Goal)*

---

## Missing But Recommended

- [ ] Define performance budgets and acceptance thresholds (CPU/memory overhead, sample latency, upload latency). `[PERF-001 to PERF-006, D-009]`
- [ ] Add security hardening for uploader path: TLS validation, token/cert handling, secret redaction in logs. `[SEC-001 to SEC-005]`
- [ ] Implement offline buffering and retry policy: queue limits, backoff, drop policy, corruption handling. `[FR-044, ERR-004]`
- [ ] Add report schema versioning and migration plan (CSV/JSON fields, backward compatibility contract). `[§11.2]`
- [ ] Add observability counters in report metadata: samples collected, parser failures, upload failures, queue depth. `[ERR-005]`
- [ ] Add rollback/feature-flag plan for staged deployment on mixed RDK device classes.
- [ ] Add parser compatibility fixtures for multiple kernel variants (`pagetypeinfo`/`buddyinfo` format drift). `[FR-002, TST-010, TST-011]`
