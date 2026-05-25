---
name: tdd
description: "Vertical-slice test-driven workflow for meminsight features and fixes, anchored to OpenSpec capabilities and fixture validation."
---

# TDD

## Purpose

Build changes in thin, behavior-first slices:

1. Write one failing behavior check.
2. Implement minimum code to pass.
3. Refactor safely.
4. Repeat.

## Meminsight-specific rules

1. Start each slice from an OpenSpec capability in openspec/specs.
2. For parser/report changes, use fixture-based checks in test/ and test/run_ut.sh.
3. Avoid implementation-coupled tests when behavior-level checks are possible.
4. Preserve existing CSV structure unless explicitly changing behavior.
5. Validate optional JSON behavior where feature flags or schema are touched.

## Slice template

- Capability: Cxx
- Behavior under test: short statement
- Failing proof: command or test path
- Minimal implementation change: files touched
- Passing proof: command or test path

## Exit criteria

- All new behavior is linked to a capability spec.
- Tests fail before and pass after implementation.
- No unrelated behavior drift introduced.
