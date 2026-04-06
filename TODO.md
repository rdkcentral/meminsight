# MemInsight TODO

## Priority Tasks
- [x] Integrate cJSON with runtime dynamic linking for JSON support.
- [x] Add CLI/config flags for JSON output format selection and `--json-pretty`.
- [ ] Add fragmentation data collection flow:
  - Check `/proc/pagetypeinfo` first; if present, collect and dump to CSV/JSON.
  - Fallback to `/proc/buddyinfo`; if present, collect and dump to CSV/JSON.
  - If neither exists, don't log any data.
- [ ] Change default output directory to `/opt/meminsight` (keep `/tmp/meminsight` as configurable fallback).
- [ ] Add upload cadence logic to trigger upload through external script/binary based on configured intervals.
- [ ] Document meminsight execution flow for both manual and automatic/systemd runs.
- [ ] Reorganize code and introduce centralized macros/constants for modular configuration.
- [ ] Add `agents/`, `skills/`, `instructions/`, and `copilot-instructions.md` in `.github/`.
- [ ] Add cleanup, upload, and backup cadence policy for existing reports.
- [ ] Add sample systemd service and sample `.bb` recipe for build/integration.

## Optional Tasks
- [ ] Add auto component release workflow with Slack approval gate.
- [ ] Develop meminsight-related MCP tools for actions such as release, test run, native/container build.
- [ ] Use memleakutils in `build_and_test.yml` for leak identification.
- [ ] Update `README.md` and `crashupload.wiki` documentation.
- [ ] Improve test organization, generation, and coverage reporting.

## Missing But Recommended
- [ ] Define performance budgets and acceptance thresholds (CPU/memory overhead, sample latency, upload latency).
- [ ] Add parser compatibility fixtures for multiple kernel variants (`pagetypeinfo`/`buddyinfo` format drift).
- [ ] Add security hardening tasks for uploader path (TLS validation, token/cert handling, secret redaction in logs).
- [ ] Add offline buffering and retry policy details (queue limits, backoff, drop policy, corruption handling).
- [ ] Add report schema versioning and migration plan (CSV/JSON fields, backward compatibility contract).
- [ ] Add observability counters/health metrics (samples collected, parser failures, upload failures, queue depth).
- [ ] Add rollback/feature-flag plan for staged deployment on mixed RDK device classes.
