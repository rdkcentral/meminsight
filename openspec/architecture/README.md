# OpenSpec Architecture Package

This folder contains architecture-level baseline artifacts to complement capability-level specs in openspec/specs.

## Files

- 00-baseline-architecture.md
  - Production-oriented baseline architecture analysis for meminsight brownfield OpenSpec adoption.
  - Includes runtime/build/dependency/reliability/test/readiness analysis.
  - Uses explicit Facts, Inferences, Assumptions, and Unknowns sections.

## Current folder interpretation

1. openspec/specs is the capability-level verified behavior layer.
2. openspec/architecture is the system-level understanding and operational reasoning layer.
3. openspec/changes is the intentional behavior-delta layer.
4. Baseline revisions update assumptions and unknowns when validation evidence is added.

## Change control guidance

- Update architecture docs when subsystem boundaries, deployment assumptions, or runtime flows materially change.
- Do not place speculative future design in capability specs.
- Prefer linking architecture findings to concrete capability IDs before implementation proposals.
