---
applyTo: "src/**/*.c,src/**/*.h"
description: "Use when implementing or reviewing meminsight C source changes, especially /proc parsing, CSV/JSON report generation, and TESTME fixture support."
---

# C Implementation Instruction

## Primary goal

Implement robust, low-risk C changes for meminsight with parser tolerance and backward-compatible reporting.

## Required behavior

- Treat `/proc` inputs as unstable: guard parsing and handle missing/malformed lines safely.
- Keep optional features optional: never hard-fail collection because fragmentation/JSON is unavailable.
- Preserve stable report contracts for existing CSV consumers.
- Keep memory and CPU overhead minimal.

## Parser checklist

- Validate `sscanf`/parse results before use.
- Bound all fixed-size buffers and arrays.
- Use conservative defaults when data is absent.
- Avoid assumptions about column count and whitespace style.

## TESTME checklist

- Add fixture support if parser input source is optional or platform-dependent.
- Ensure tests cover preferred path and fallback path.
- Keep test fixtures deterministic and repository-local.
