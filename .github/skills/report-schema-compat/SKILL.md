---
name: report-schema-compat
description: "Use when changing CSV/JSON report fields to preserve backward compatibility and document schema-impacting changes clearly."
---

# Report Schema Compatibility Skill

## Purpose

Guard report-consumer compatibility when modifying output fields/sections.

## Workflow

1. Identify impacted CSV headers and section layout.
2. Keep old fields stable unless explicitly deprecated.
3. For JSON, keep key naming consistent and avoid unnecessary nesting churn.
4. Validate output with fixture-driven tests.
5. Update README and TODO when user asks for documentation updates.

## Compatibility rules

- Do not silently rename/remove existing fields.
- Additive changes are preferred to disruptive changes.
- If behavior changes, explain migration impact in docs.
