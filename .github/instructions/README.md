# Instructions Directory

Instruction files define always-on or file-pattern-specific coding guidance.

## Files

- `c-implementation.instructions.md`
  - Scope: `src/**/*.c`, `src/**/*.h`
  - Focus: parser tolerance, memory-safe C patterns, CSV/JSON compatibility, TESTME discipline.

- `build-and-ci.instructions.md`
  - Scope: build scripts, CI workflows, deployment samples.
  - Focus: reproducible builds, explicit feature flags, workflow separation, script-first verification.

## Why these exist

- Reduce ambiguity for repeated changes.
- Keep implementation and CI behavior consistent across contributors.
- Encode project ideology without over-constraining contributors.
