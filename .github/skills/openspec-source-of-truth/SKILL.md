---
name: openspec-source-of-truth
description: "Use for implementation tasks to enforce OpenSpec-first behavior, As-Is alignment, and change-spec gating for behavior changes."
---

# OpenSpec Source of Truth Skill

## Purpose

Ensure every implementation task follows the OpenSpec SDD workflow in this repository.

## Required workflow

1. Identify impacted capability specs in openspec/specs before editing code.
2. Implement only behavior already defined in openspec/specs unless an approved change requires new behavior.
3. For behavior changes, create or update delta-spec content in openspec/changes before implementation changes.
4. Keep acceptance checks tied to documented capability behavior.
5. If non-OpenSpec documentation conflicts with openspec/specs, follow openspec/specs and flag the discrepancy.

## Completion checks

- Implementation and specs remain in 1:1 parity for touched capabilities.
- No speculative or future-state language is introduced into baseline specs.
- Baseline onboarding keeps openspec/changes free of active deltas unless a behavior change is intentionally in progress.
