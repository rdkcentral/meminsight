# Agents and Skills Usage Guide

This guide explains how to use meminsight agent modes and skills in day-to-day work.

## Core operating rule

1. Always start from OpenSpec capabilities under openspec/specs.
2. Use openspec/changes for intentional behavior changes.
3. Use openspec/architecture for system-level context before larger changes.
4. Keep implementation and capability documentation synchronized.

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

1. Run openspec-explore and grill-with-docs-openspec to resolve scope and acceptance behavior.
2. Run openspec-propose (or create the related delta manually) under openspec/changes.
3. Run to-issues-openspec to split work into capability-linked vertical slices.
4. Implement slices with openspec-apply-change plus tdd and keep tests/specs synchronized.
5. Run MemInsight Reviewer mode and address findings.
6. Promote updated behavior into openspec/specs after approval.
7. Run openspec-archive-change when the change is complete.

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

### openspec-explore

Use when clarifying requirements, risks, and subsystem boundaries before implementation.

### openspec-propose

Use when converting a feature idea into a complete OpenSpec change artifact set.

### openspec-apply-change

Use when implementing tasks from an approved OpenSpec change.

### openspec-archive-change

Use when implementation and review are complete and the change is ready to archive.

## Suggested invocation order

1. zoom-out (if area is unfamiliar)
2. openspec-source-of-truth
3. openspec-explore and grill-with-docs-openspec (for larger changes)
4. openspec-propose and to-issues-openspec (for new deltas)
5. openspec-apply-change with tdd or diagnose
6. meminsight-reviewer agent mode
7. openspec-archive-change

## Prompt shortcuts

1. /opsx:propose
2. /opsx:explore
3. /opsx:apply
4. /opsx:archive

## Evidence checklist before merge

- Capability ID mapping is explicit.
- Relevant tests or fixtures were run.
- Compatibility impact is stated.
- OpenSpec docs remain accurate for changed behavior.

## Minimal command checklist

Use project-appropriate commands; common examples are:

1. `sh cov_build.sh --clean`
2. `sh cov_build.sh --enable-cjson --test`
3. `sh test/run_ut.sh`
4. Targeted fixture test command when validating a specific parser edge case
