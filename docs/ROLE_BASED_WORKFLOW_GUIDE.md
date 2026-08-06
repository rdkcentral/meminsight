# Role-Based Workflow Guide

This guide defines how each stakeholder role works with OpenSpec, code, tests, and documentation.

## 1. Reviewer

Primary intent: review and approve code changes for features, fixes, and bugs.

Responsibilities:

1. Validate conformance to OpenSpec capability specs.
2. Validate compatibility impact for CSV and optional JSON output.
3. Validate regression evidence from fixtures and tests.
4. Block merges with unresolved high-risk findings.

Typical flow:

1. Map changed files to capability IDs.
2. Check for required openspec/changes entries when behavior changes exist.
3. Review tests for positive and negative coverage.
4. Approve or request changes with severity-ranked findings.

## 2. Developer or Contributor

Primary intent: maintain and improve code quality and deliver fixes/features.

Responsibilities:

1. Implement behavior aligned to OpenSpec capabilities.
2. Keep parser/report compatibility constraints intact unless approved change requires otherwise.
3. Add or update fixture-backed tests for changed behavior.
4. Keep docs and implementation synchronized.

Typical flow:

1. Use zoom-out and openspec-source-of-truth.
2. Implement via tdd for behavior changes, or diagnose for regressions.
3. Run tests and summarize evidence.
4. Request review with capability linkage.

## 3. Architect Owner

Primary intent: define and shape effort direction and feature plans.

Responsibilities:

1. Define problem statements and capability impact clearly.
2. Introduce behavior changes via openspec/changes before coding.
3. Ensure plans are sliced into independently deliverable work.
4. Align implementation direction with long-term architecture quality.

Typical flow:

1. Use openspec-explore and grill-with-docs-openspec for requirement refinement.
2. Use openspec-propose for proposal/design/tasks initialization.
3. Use to-issues-openspec for capability-linked issue slicing.
4. Track adoption, parity updates, and archive closure after merge.

## 4. Tester

Primary intent: validate the component end-to-end, including corner cases.

Responsibilities:

1. Build test coverage around capability acceptance behavior.
2. Validate corner cases across parser sources and optional feature flags.
3. Confirm evidence for both normal and negative paths.
4. Communicate risk to reviewer, developer, and architect stakeholders.

Typical flow:

1. Map test intent to capability IDs.
2. Execute fixture and integration flows.
3. Report pass/fail with reproduction details.
4. Feed failures into diagnose workflow.

## 5. Technical Documentation Expert

Primary intent: maintain technically accurate and audience-appropriate documentation.

Responsibilities:

1. Keep OpenSpec docs technically exact and implementation-aligned.
2. Keep contributor docs clear for both engineering and non-engineering readers.
3. Maintain role guides, usage guides, and architectural rationale clarity.
4. Remove ambiguous or speculative wording in baseline docs.
5. Keep prompt/skill lifecycle docs aligned with active OpenSpec workflows.

Typical flow:

1. Sync docs with merged behavior and capability updates.
2. Cross-check with parity matrix and implementation anchors.
3. Publish concise summaries for management and detailed notes for engineers.

## Collaboration model

1. Architect defines change intent and capability impact.
2. Developer implements and provides test evidence.
3. Tester validates edge and end-to-end behavior.
4. Reviewer enforces standards/spec conformance and approves.
5. Documentation expert updates narrative and reference docs.

## OpenSpec lifecycle tools

1. Propose: openspec-propose or /opsx:propose
2. Explore: openspec-explore or /opsx:explore
3. Apply: openspec-apply-change or /opsx:apply
4. Archive: openspec-archive-change or /opsx:archive
