# Software Requirements Specification (SRS)

## Project
MemInsight Enhancement for RDK Devices: Memory Fragmentation Collection, Structured Reporting, and Near Real-Time Log Analytics Integration

## Version
- Document version: 1.1-working
- Date: 2026-04-07
- Status: Working draft with implementation status annotations

## 1. Purpose
This document defines software requirements for extending MemInsight on RDK devices to collect Linux memory fragmentation data, generate structured reports, and enable near real-time delivery to centralized Log Analytics via a lightweight architecture suitable for production devices.

This SRS is intentionally explicit, testable, and implementation-oriented. Any unresolved point is marked as TBD and captured in the decision log.

### 1.1 Implementation Snapshot (As Of 2026-04-07)
Status legend:
- Implemented: requirement text is shown with strike-through.
- Partial: implemented in limited scope (for example CI helper scripts) but not as full product behavior.
- Pending: not implemented yet.

Feature: Memory fragmentation profiling
- ~~Given the device is running a supported RDK-B or RDK-E build~~
- ~~When a memory capture is initiated~~
- ~~Then the system MUST execute MemInsight~~
- ~~And collect memory usage and fragmentation metrics~~
- Status: Implemented in collector and report paths (CSV and JSON).

Feature: Scheduled Report Upload and Lifecycle Management
- Upload responsibility is fully delegated to an external sidecar component.
- MemInsight writes reports to the output directory; it does not upload, buffer for network, or manage endpoint connectivity.
- Status: MemInsight obligation complete (clean output directory on startup, structured reports written). Sidecar uploader implementation is out of scope for this SRS.

Feature: Pre-capture cleanup
- Given previous memory capture reports exist locally
- When a new memory capture is initiated
- And a report upload cadence is configured
- Then existing reports MUST be cleared before new reports are written
- And the new capture MUST produce a clean, isolated report set
- Status: Partial. Implemented in leak-detection helper flow (`scripts/detect_leak.sh`) for CI runs, not as a general MemInsight product behavior.

Feature: MemInsight execution during pre-release QA testing
- Given a pre-release qualification image is under relQA testing
- And MemInsight is available as a downloadable package
- When the image is prepared for test execution
- ~~Then MemInsight MUST be executed before the start of relQA test cases~~
- When all relQA test cases are completed
- ~~And the reports MUST be available for pipeline-based analysis~~
- And memory-related anomalies MUST be identified and highlighted
- Status: Partial. CI flow executes MemInsight/memleak tooling and publishes report artifacts/logs; full relQA policy enforcement and anomaly triage workflow remain open.

Feature: Pre-Release Baseline Comparison
- Given memory baselines exist for a prior validated release
- When MemInsight data is generated for a new build
- Then memory usage and fragmentation metrics must be compared
- And regressions must be clearly highlighted
- Status: Pending.

Feature: OTA Delivery of Memory Analysis Tooling
- Given a device is running a pre-release image
- When an OTA containing MemInsight updates is triggered
- Then the device must successfully download and apply the update
- And existing profiling workflows must remain functional
- Status: Pending.

Stretch Goal
Feature: AI-Powered Analysis of Memory Reports
- Given MemInsight reports are generated from memory captures
- When AI-based analysis is invoked
- Then patterns, anomalies, and regressions must be identified
- And findings must be summarized in human-readable insights
- Status: Pending.

## 2. Scope
In scope:
- Fragmentation telemetry ingestion from procfs.
- Structured output in CSV (existing) and JSON (new).
- Pre-capture output directory sanitization.
- Storage and retention management on device.
- Service/runtime integration constraints for RDK platforms.
- Security, performance, test, logging, and release automation requirements.
- Documentation updates.

Out of scope:
- Upload, network transport, and endpoint lifecycle management (delegated to external sidecar).
- Backend Log Analytics schema changes.
- Fleet policy management implementation details outside MemInsight.
- Exact Yocto layer organization (captured as integration requirement only).

## 3. Stakeholders
- System Performance Engineering
- RDK Platform Engineering (Broadband and Video)
- QA and Validation
- Security Engineering
- Release Engineering / DevOps

## 4. Context and Problem Statement
Current MemInsight focuses on process/system memory metrics and CSV reporting. The new requirement is to add fragmentation observability for proactive detection of allocation risks and production memory behavior drift, while preserving low overhead and operational robustness across heterogeneous RDK Linux variants.

## 5. Definitions and Terms
- Buddy info: `/proc/buddyinfo`, per-zone free page counts by order.
- Pagetype info: `/proc/pagetypeinfo`, detailed free pages by migration type and order.
- Order `n`: block size of `2^n` pages.
- Structured report: machine-parsable CSV and/or JSON output.
- Uploader: separate component (binary/script/service) that pushes reports to endpoint.
- Near real-time: periodic delivery with configurable interval, target typically seconds to minutes.

## 6. System Overview
### 6.1 High-Level Components
1. MemInsight collector (existing binary, enhanced)
2. Local report store and buffer manager
3. Optional JSON serialization plugin path (runtime cJSON loading)
4. External uploader component (new binary/script/systemd unit)
5. Log Analytics endpoint integration through existing logupload endpoint

### 6.2 Architectural Principle
MemInsight remains lightweight: collection and serialization in MemInsight; upload responsibility delegated to a dedicated uploader path.

## 7. Assumptions and Constraints
1. RDK device kernels and procfs formats are not fully uniform.
2. Some devices may not expose all procfs files (for example, missing pagetype or platform DDR files).
3. MemInsight currently supports CSV and must preserve backward compatibility.
4. Secure transport and auth are mandatory for any upload path.
5. NTP synchronization is required for trustworthy timestamps.

## 8. Functional Requirements

### 8.1 Fragmentation Data Collection
- FR-001 (MUST): MemInsight shall collect memory fragmentation data by parsing `/proc/buddyinfo`.
- FR-002 (MUST): MemInsight shall support kernel-format variance handling for `/proc/buddyinfo` with strict parser validation and graceful fallback behavior.
- FR-003 (SHOULD): MemInsight should parse `/proc/pagetypeinfo` when present to provide richer fragmentation diagnostics.
- FR-004 (MUST): MemInsight shall expose a configuration mode to select data source policy:
  - `buddyinfo-only`
  - `pagetypeinfo-preferred-with-buddyinfo-fallback`
  - `collect-both`
- FR-005 (MUST): If selected source file is unavailable or malformed, MemInsight shall continue execution, log a bounded warning, and emit explicit "source_unavailable" or "parse_error" status fields.
- FR-006 (MUST): Existing memory collection behavior remains: prefer `smaps_rollup` when present; otherwise use `smaps`. If `-s` is passed, MemInsight shall attempt `smaps_rollup` first and, if unavailable, log and fallback to `smaps`.

### 8.2 Derived Metrics
- FR-010 (MUST): MemInsight shall derive at least the following fragmentation metrics from available source data:
  - total free pages by order
  - high-order free page availability summary
  - fragmentation index/ratio fields (formula documented in implementation spec)
  - zone-level visibility when available
- FR-011 (MUST): Every derived metric must include a deterministic computation definition in code comments and test references.
- FR-012 (MUST): Metric calculation shall tolerate missing zones/orders without process crash.

### 8.3 Report Generation
- FR-020 (MUST): MemInsight shall continue generating CSV reports.
- FR-021 (MUST): MemInsight shall support JSON output format.
- FR-022 (MUST): JSON support shall be runtime-loaded using cJSON only when JSON output is requested by flag/config; build shall not hard-link JSON dependency for CSV-only usage.
- FR-023 (MUST): MemInsight shall support `json_pretty` formatting via dedicated runtime flag.
- FR-024 (MUST): Report metadata shall include:
  - timestamp (UTC)
  - uptime
  - device identifier
  - firmware (if available)
  - collection source mode and parser status
- FR-025 (MUST): CSV headers shall be defined through centralized macros/constants, not hardcoded inline in data-writing functions.

### 8.4 Platform-Specific Optional Telemetry
- FR-030 (MUST): MemInsight shall probe for Amlogic DDR bandwidth source file using a global capability marker at runtime.
- FR-031 (MUST): If DDR bandwidth source is present and readable, corresponding fields shall be captured.
- FR-032 (MUST): If absent, MemInsight shall skip DDR fields without failure and indicate capability absence in metadata.

### 8.5 Upload Delegation
- FR-040 (MUST): MemInsight shall not upload to any endpoint. Upload is the sole responsibility of an external sidecar component.
- FR-040a (MUST): MemInsight's only obligation toward the upload pipeline is writing complete, isolated report sets to the configured output directory.
- Note: All upload scheduling, buffering, retry, batching, and endpoint lifecycle requirements are captured in the sidecar component specification, not this document.

### 8.6 Storage and Retention
- FR-050 (MUST): Default persistent output directory for video platforms shall be `/opt/meminsight`.
- FR-051 (MUST): Existing `/tmp/meminsight` behavior may remain configurable for non-persistent/legacy modes.
- FR-052 (MUST): Rotation policy shall execute on startup/reboot and by retention cadence.
- FR-053 (MUST): Rotation shall support archive compression (tar/gzip or equivalent) before deletion.
- FR-054 (MUST): Retention limits (max age, max size, max files) shall be configurable.

### 8.7 Service Dependencies and Time
- FR-060 (MUST): MemInsight service shall declare NTP synchronization dependency before regular reporting starts.
- FR-061 (MUST): If NTP is not synchronized at startup, system behavior shall be configurable:
  - wait-with-timeout,
  - run-with-unsynced-time-flag,
  - defer upload until sync.

### 8.8 Logging and Debug Build
- FR-070 (MUST): Production logging shall remain minimal and low overhead.
- FR-071 (MUST): Debug logging shall be gated behind compile-time `ENABLE_DEBUG` macro.
- FR-072 (MUST): Yocto recipe passing `--debug` shall enable `ENABLE_DEBUG` and include debug logging paths.
- FR-073 (SHOULD): Debug logs should be structured and rate-limited for repeated parser/upload faults.

### 8.9 Documentation and Release Automation
- FR-080 (MUST): `README.md` shall be updated to reflect new features, flags, storage, and uploader architecture.
- FR-081 (MUST): `meminsight.wiki` shall be updated with operator-level and developer-level guidance.
- FR-082 (MUST): CI/CD workflow shall support auto-release process after merge to develop:
  - detect latest release version
  - infer bump type (major/minor/patch) from PR metadata
  - trigger gitflow release process
  - notify maintainers via Slack and/or email
  - support explicit approval/rejection gate
  - on rejection, terminate gracefully without partial release state

## 9. Security Requirements
- SEC-001 (MUST): Upload transport shall use TLS.
- SEC-002 (MUST): Authentication shall support token-based and certificate-based mechanisms.
- SEC-003 (MUST): Secrets/tokens/cert paths shall not be hardcoded.
- SEC-004 (MUST): Sensitive values shall not be logged in plaintext.
- SEC-005 (SHOULD): Certificate validation failures should be observable with non-sensitive diagnostic reason codes.

## 10. Performance and Scalability Requirements
- PERF-001 (MUST): Collection path shall remain lightweight and non-blocking relative to baseline system behavior.
- PERF-002 (SHOULD): Memory data collection should complete within milliseconds per sampling interval under normal load.
- PERF-003 (SHOULD): CPU overhead should remain negligible at normal sampling frequencies.
- PERF-004 (MUST): Upload or uploader failures shall not block collection loop.
- PERF-005 (MAY): Batching and compression may be used to reduce network overhead.
- PERF-006 (SHOULD): Design should scale across Broadband and Video platform variants.

Note: Exact numeric thresholds for "measurable" and "negligible" are TBD and must be finalized with Performance Engineering before GA.

## 11. External Interfaces

### 11.1 CLI/Config Additions (Proposed)
- `--frag-source <mode>` where mode in {`buddyinfo-only`, `pagetypeinfo-preferred-with-buddyinfo-fallback`, `collect-both`}
- `--format <csv|json|both>`
- `--json-pretty`
- `--output-dir <path>`
- `--retention-max-age <hours>`
- `--retention-max-size <MB>`
- `--retention-max-files <count>`

### 11.2 Output Schema Requirements
- CSV schema version field required.
- JSON schema version field required.
- Mandatory keys for parser mode/status and collection health.

## 12. Error Handling and Resilience
- ERR-001 (MUST): Missing procfs source file shall not crash collector.
- ERR-002 (MUST): Parse errors shall be isolated per sample and reported in metadata.
- ERR-003 (SHOULD): Component health counters should be exposed in report metadata.

Note: Endpoint outage resilience, buffer saturation policy, and retry behavior are uploader-sidecar concerns and are not tracked here.

## 13. Test Requirements and Validation Matrix

### 13.1 Backward Compatibility
- TST-001 (MUST): All existing MemInsight functionalities continue to pass current unit and functional tests.

### 13.2 Unit Tests
- TST-010 (SHOULD): Validate `/proc/buddyinfo` parsing against representative format variants across kernel versions.
- TST-011 (SHOULD): Validate `/proc/pagetypeinfo` parser with varied migration-type layouts.
- TST-012 (MUST): Validate metric derivation formulas with deterministic fixture inputs.
- TST-013 (MUST): Validate JSON runtime-loading path and CSV-only path without cJSON dependency.

### 13.3 Functional Tests
- TST-020 (MUST): Verify correctness of derived fragmentation metrics.
- TST-021 (MUST): Verify source selection policy and fallback behavior.
- TST-022 (MUST): Verify `-s` smaps behavior and fallback logging.
- TST-023 (MUST): Verify uptime appears in CSV top header data.
- TST-024 (MUST): Verify macro-based CSV header integrity and ordering.

### 13.4 Fault Injection
- TST-030 (SHOULD): `/proc/buddyinfo` unavailable.
- TST-031 (SHOULD): `/proc/pagetypeinfo` unavailable.
- TST-033 (MUST): output directory full/permission errors.
- TST-034 (SHOULD): NTP unsynchronized startup conditions.

### 13.5 Performance Tests
- TST-040 (MUST): Ensure collection overhead remains within agreed limits.
- TST-041 (SHOULD): Compare CPU/memory overhead with and without fragmentation collection.

## 14. Operational and Deployment Requirements
- OPS-001 (MUST): Service units/scripts shall include dependency ordering for network and NTP readiness.
- OPS-002 (MUST): Output directory shall be configurable per platform profile.
- OPS-003 (MUST): Rotation/retention configuration shall be externally tunable.

## 15. Decision Log (Open/TBD)
1. D-001: Final policy for buddyinfo vs pagetypeinfo default mode.
   - Status: Resolved and implemented.
   - Chosen default: pagetype preferred with buddyinfo fallback.
   - Options considered: buddyinfo-only (minimal), pagetype preferred (richer), collect-both (maximum visibility).
2. D-002: Final fragmentation index formulas and thresholds.
3. D-003: Rotation cadence details (startup-only, periodic, and reboot interaction).
4. D-004: Archive format selection (`tar.gz` recommended) and max archive size.
5. D-005: Final CI release approval UX (Slack interactive action, mail-based approval, or both).
6. D-006: Exact definition of acceptable overhead thresholds.

Note: Upload interval defaults, buffer saturation policy, and uploader implementation form are decisions for the sidecar component specification.

## 16. Acceptance Criteria Traceability
1. Collect fragmentation from `/proc/buddyinfo`: FR-001, FR-002, TST-010.
2. Structured report generation (CSV/JSON): FR-020 through FR-024, TST-013.
3. Pre-capture output directory sanitization: FR-052, TST-033.
4. Low overhead: PERF-001 through PERF-004, TST-040.
5. Upload delegation to sidecar: FR-040.

Note: Upload pipeline acceptance criteria (endpoint delivery, retry, batching) are tracked in the sidecar component specification.

## 17. Proposed Implementation Phases
1. Phase 1: Parser and metrics foundation
   - buddyinfo parser hardening
   - optional pagetype parser
   - derived metrics and fixtures
2. Phase 2: Report schema and JSON runtime loading
   - macro-based CSV headers
   - JSON output and pretty mode
3. Phase 3: Storage management and pre-capture sanitization
   - persistent output path defaults
   - pre-capture directory wipe on startup
   - rotation and compression
4. Phase 4: Platform polish and release automation
   - Amlogic DDR optional metrics
   - debug build gating
   - README/wiki updates
   - automated release workflow

Note: Sidecar uploader implementation phases are tracked separately.

## 18. Non-Goals and Risk Notes
- This SRS does not force upload implementation inside MemInsight.
- Heterogeneous kernel formats are a known risk; parser extensibility and fixture breadth are mandatory mitigation.
- Metrics misinterpretation risk is mitigated by formula documentation and traceable tests.

## 19. Required Documentation Deliverables
1. Developer design note for parser/source policy and metric formulas.
2. Operator guide for new flags, output locations, and retention.
3. Release engineering runbook for automated release approval flow.

Note: TLS/auth and secret-handling documentation belongs in the sidecar uploader specification.

## 20. Approval Checklist
- Performance Engineering approved overhead thresholds.
- Security Engineering approved TLS/auth implementation.
- Platform Engineering approved directory, retention, and service dependencies.
- QA approved validation matrix and fixtures.
- Release Engineering approved workflow automation and approval gates.
