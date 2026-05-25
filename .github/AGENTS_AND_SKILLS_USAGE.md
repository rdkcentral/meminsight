# Agents and Skills Usage Guide

This guide explains how to use meminsight agent modes and skills in day-to-day work.

## Core operating rule

1. Always start from OpenSpec capabilities under openspec/specs.
2. Use openspec/changes for intentional behavior changes.
3. Keep implementation and capability documentation synchronized.

## Agent modes

### MemInsight Implementer

File: .github/agents/meminsight-implementer.agent.md

Use for feature implementation and bug fixes in src/, test/, scripts/, and workflows.

### MemInsight Reviewer

File: .github/agents/meminsight-reviewer.agent.md

Use for standards/spec conformance reviews, compatibility risk checks, and test coverage checks.

## How to use: step-by-step

### A. Implement a bug fix (no intended behavior change)

1. Run zoom-out if the code area is unfamiliar.
2. Run openspec-source-of-truth and identify impacted capability IDs.
3. Implement fix with tdd or diagnose depending on whether issue is a known behavior gap or uncertain defect.
4. Add or update fixture coverage in test/ and run test evidence commands.
5. Run MemInsight Reviewer mode for standards/spec conformance.
6. Confirm no new delta spec is required in openspec/changes.

### B. Implement an intentional behavior change

1. Run grill-with-docs-openspec to resolve scope and acceptance behavior.
2. Create the related delta under openspec/changes before implementation.
3. Run to-issues-openspec to split work into capability-linked vertical slices.
4. Implement slices with tdd and keep tests and specs synchronized.
5. Run MemInsight Reviewer mode and address findings.
6. Promote updated behavior into openspec/specs after approval.

### C. Perform a review-only workflow

1. Identify changed files and map to capability IDs.
2. Run MemInsight Reviewer mode.
3. Verify compatibility and test evidence are explicit.
4. Approve only when standards, specs, and evidence checks pass.

## Skills

### openspec-source-of-truth

Use before edits to confirm capability ownership and requirements source.

### diagnose

Use for bug investigations with deterministic reproduction and fixture-first regression checks.

### tdd

Use for behavior-first, vertical-slice development tied to capability IDs.

### to-issues-openspec

Use to break plans into implementation issues mapped to OpenSpec capability IDs.

### zoom-out

Use for onboarding and system-level code maps before deep changes.

### grill-with-docs-openspec

Use for larger changes that need requirement clarification and stakeholder alignment.

### proc-fragmentation-compat

Use when touching buddyinfo/pagetypeinfo parsing and fallback handling.

### report-schema-compat

Use when changing CSV/JSON output fields or section layout.

## Suggested invocation order

1. zoom-out (if area is unfamiliar)
2. openspec-source-of-truth
3. grill-with-docs-openspec (for larger changes)
4. tdd or diagnose
5. meminsight-reviewer agent mode

## Evidence checklist before merge

- Capability ID mapping is explicit.
- Relevant tests or fixtures were run.
- Compatibility impact is stated.
- OpenSpec docs remain accurate for changed behavior.

## Minimal command checklist

Use project-appropriate commands; common examples are:

1. sh cov_build.sh --test
2. sh test/run_ut.sh
3. Targeted fixture test command when validating a specific parser edge case
