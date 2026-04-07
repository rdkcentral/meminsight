# Agents Directory

This directory contains custom agent modes for meminsight.

## Files

- `meminsight-implementer.agent.md`
  - Purpose: implementation-focused agent for C feature work, parser resilience, report compatibility, and test updates.
  - Use when: adding/fixing functionality in `src/`, `scripts/run_ut.sh`, build scripts, and workflows.

## Design notes

- Keep agents task-specific and low-count.
- Avoid overlapping agent responsibilities.
- Prefer one primary implementer agent unless clear workflow separation is needed.
