---
name: to-issues-openspec
description: "Break plans into implementation issues linked directly to OpenSpec capability IDs and acceptance checks."
---

# To Issues (OpenSpec)

## Purpose

Convert feature plans or fixes into independently executable issues with explicit OpenSpec linkage.

## Required inputs

1. Source plan or request.
2. Impacted capability files in openspec/specs.
3. If behavior changes are intended, corresponding delta entries in openspec/changes.

## Output rules

1. Every issue must include a capability mapping section.
2. Every issue must include acceptance criteria tied to observable behavior.
3. Issue slices should be vertical and independently verifiable.

## Issue template

### Title

Short, behavior-oriented title.

### Capability linkage

- Primary capability: Cxx
- Related capabilities: Cyy, Czz
- Spec files: relative paths under openspec/specs

### What to build

Concrete, user-observable behavior target.

### Acceptance criteria

- [ ] Criterion 1
- [ ] Criterion 2
- [ ] Criterion 3

### Test proof

Commands or fixture paths to validate behavior.

### Dependencies

Blocking issues or None.

## Exit criteria

- 1:1 mapping between planned work and capability references is visible.
- Issue boundaries support independent execution and review.
