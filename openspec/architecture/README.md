# OpenSpec Architecture Package

This folder contains architecture-level baseline artifacts to complement capability-level specs in openspec/specs.

## Files

- 00-baseline-architecture.md
  - Production-oriented baseline architecture analysis for meminsight brownfield OpenSpec adoption.
  - Includes runtime/build/dependency/reliability/test/readiness analysis.
  - Uses explicit Facts, Inferences, Assumptions, and Unknowns sections.

## How this folder should evolve

1. Keep openspec/specs focused on current verified behavior (capability-level truth).
2. Keep openspec/architecture focused on system-level understanding and operational reasoning.
3. Keep openspec/changes focused on intentional behavior deltas.
4. When architecture assumptions are validated, promote them to Facts in the next baseline revision.

## Change control guidance

- Update architecture docs when subsystem boundaries, deployment assumptions, or runtime flows materially change.
- Do not place speculative future design in capability specs.
- Prefer linking architecture findings to concrete capability IDs before implementation proposals.
