---
applyTo: "configure.ac,Makefile.am,cov_build.sh,scripts/run_ut.sh,.github/workflows/**/*.yml,deploy/**/*"
description: "Use when changing build scripts, test runners, CI workflows, or deployment samples for meminsight."
---

# Build and CI Instruction

## Build expectations

- Keep build flags explicit: `--enable-cjson`, `--test`.
- Build scripts must support clean reproducible runs.
- Avoid hidden feature toggles; reflect all behavior in help text.

## CI expectations

- Separate responsibilities:
  - build-only workflow
  - unit-test workflow
- Install only required dependencies.
- Use repository scripts (`cov_build.sh`, `scripts/run_ut.sh`) as single source of truth.

## Deployment sample expectations

- Keep systemd and Yocto examples minimal and readable.
- Samples should track current binary name, default paths, and feature flags.
- Document where each sample lives and how to adapt it.
