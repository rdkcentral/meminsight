---
name: diagnose
description: "Disciplined diagnosis loop for meminsight parser bugs, runtime failures, and regressions using deterministic fixture-first feedback."
---

# Diagnose

## Purpose

Debug meminsight issues through a strict loop:

1. Reproduce
2. Minimize
3. Hypothesize
4. Instrument
5. Fix
6. Regression test

## Meminsight-specific rules

1. Start from deterministic repro whenever possible using test fixtures in test/.
2. Validate behavior against impacted OpenSpec capability files in openspec/specs.
3. Prefer parser-focused probes over broad logging.
4. Keep temporary debug instrumentation clearly tagged and removable.
5. Confirm CSV compatibility and optional JSON fallback behavior after fix.

## Suggested workflow

1. Create a fast pass/fail loop using `sh test/run_ut.sh` or targeted `--test` fixture runs.
2. Capture 3 to 5 ranked hypotheses before modifying production code.
3. Add narrow instrumentation for one hypothesis at a time.
4. Implement minimal fix.
5. Add or update fixture coverage for regression protection.
6. Re-run the original reproduction path and tests.

## Exit criteria

- Original bug path no longer reproduces.
- New or updated fixture-based check passes.
- Added debug instrumentation is removed.
- OpenSpec capability docs remain accurate for touched behavior.
