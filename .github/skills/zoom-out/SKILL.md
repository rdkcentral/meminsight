---
name: zoom-out
description: "Provide a system-level map of meminsight modules, call flow, and capability ownership for onboarding and design clarity."
---

# Zoom Out

## Purpose

Generate high-level understanding before detailed implementation or review.

## Output expectations

1. Module map of core files and responsibilities.
2. End-to-end data flow through collection, parsing, aggregation, and reporting.
3. OpenSpec capability ownership map to implementation areas.
4. Key compatibility constraints and test coverage locations.

## Meminsight map anchors

- src/meminsight.c and src/meminsight.h
- test/ fixtures and run_ut.sh
- openspec/specs capability library
- .github instructions, skills, and agent modes

## Usage

Use when onboarding a contributor, planning a change, or reviewing unfamiliar code paths.
