---
name: proc-fragmentation-compat
description: "Use when implementing or reviewing pagetypeinfo/buddyinfo collection with format drift tolerance, fallback behavior, and low-overhead parsing."
---

# Proc Fragmentation Compatibility Skill

## Purpose

Implement fragmentation collection that is robust across kernel and platform variance.

## Workflow

1. Prefer `/proc/pagetypeinfo`.
2. Fallback to `/proc/buddyinfo`.
3. If both are unavailable/unparseable, continue collection without crash.
4. Keep parser bounded and avoid dynamic heavy allocations.
5. Add fixture-based tests for preferred and fallback paths.

## Output principles

- CSV and JSON should both include fragmentation data when available.
- Preserve existing report sections and ordering as much as possible.
- Emit explicit source metadata for JSON.
