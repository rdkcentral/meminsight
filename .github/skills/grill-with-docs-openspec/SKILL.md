---
name: grill-with-docs-openspec
description: "Structured requirement interview for larger changes, grounded in OpenSpec capabilities and repository constraints."
---

# Grill With Docs (OpenSpec)

## Purpose

Run a focused question loop for larger changes so implementation starts with unambiguous requirements.

## Required sources

1. Relevant OpenSpec capability files under openspec/specs.
2. Existing repo instructions and constraints in .github/copilot-instructions.md.
3. Impacted code and test paths.

## Interview rules

1. Ask one concrete question at a time.
2. Prefer evidence from code/specs before asking a question.
3. Resolve terminology against existing capability language.
4. Escalate conflicts between docs and specs explicitly.
5. For behavior changes, require proposed delta in openspec/changes before code updates.

## Deliverables

1. Decision log for resolved questions.
2. Confirmed scope and out-of-scope list.
3. Capability-impact matrix.
4. Test strategy aligned to parser/report risk.

## Exit criteria

- Requirements are specific enough to implement without guessing.
- OpenSpec linkage is explicit for every behavior change.
- Stakeholder review points are identified before coding starts.
