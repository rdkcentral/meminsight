# Agents Directory

This directory contains custom agent modes for meminsight.

## Files

- `meminsight-implementer.agent.md`
  - Purpose: implementation-focused agent for C feature work, parser resilience, report compatibility, and test updates.
  - Use when: adding/fixing functionality in `src/`, `test/run_ut.sh`, build scripts, and workflows.

- `meminsight-reviewer.agent.md`
  - Purpose: review-focused agent for standards and OpenSpec conformance, compatibility risk checks, and test sufficiency checks.
  - Use when: reviewing features, bug fixes, parser changes, and report schema updates before merge.

## Design notes

- Keep agents task-specific and low-count.
- Avoid overlapping agent responsibilities.
- Keep implementation and review responsibilities separated for clearer quality gates.

## Related guide

- `.github/AGENTS_AND_SKILLS_USAGE.md`
  - End-to-end usage flow for agent modes and skills.
