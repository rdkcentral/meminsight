# OpenSpec Adoption Workflow (Architecture-first, Brownfield)

This workflow extends existing OpenSpec usage by adding architecture-first intake for larger changes.

## Goal

Create repeatable behavior for planning and implementing changes in a brownfield embedded diagnostics component with minimal regression risk.

## Skill and mode orchestration

### Facts
- Existing agent modes and skills in this repository already support implementation, review, diagnosis, TDD, and OpenSpec alignment.
- Existing capability specs C01-C12 and parity matrix provide implementation anchors.

### Inferences
- A larger change intake process should start with architecture and unknowns classification before direct coding.

### Assumptions
- Teams will continue using skills/playbooks from .github assets.

### Unknowns / Manual validation required
- Team-specific role boundaries and review gates may vary and should be explicitly agreed.

## Recommended orchestration by phase

1. Baseline understanding phase
   - zoom-out
   - openspec-source-of-truth
   - Read openspec/architecture/00-baseline-architecture.md

2. Scope and requirement clarification phase
   - openspec-explore
   - grill-with-docs-openspec
   - For broad deltas, openspec-propose

3. Planning phase
   - to-issues-openspec
   - openspec-apply-change (for implementation-ready slices)

4. Implementation phase
   - tdd (default for behavior work)
   - diagnose (default for uncertain regressions)
   - proc-fragmentation-compat when touching buddyinfo/pagetypeinfo behavior
   - report-schema-compat when touching CSV/JSON fields

5. Review and closure phase
   - MemInsight Reviewer mode
   - openspec-archive-change after approval and baseline promotion

## Architectural intake checklist for new change proposals

1. Does the proposal identify impacted capability IDs?
2. Does the proposal identify subsystem boundaries affected?
3. Are Facts/Inferences/Assumptions/Unknowns explicitly listed?
4. Are compatibility and fallback expectations explicit?
5. Is manual validation scope identified for unknown platform dependencies?

## Acceptance criteria themes to reuse

1. Parser compatibility under procfs format variance.
2. CSV schema backward compatibility by default.
3. JSON optionality and graceful fallback behavior.
4. Operational resilience with missing optional sources.
5. Deterministic fixture coverage for parser-sensitive behavior.
