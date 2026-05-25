# Skills Directory

Skills are reusable task playbooks invoked when specific work appears.

## Skills

- `openspec-source-of-truth/`
  - Enforces OpenSpec-first execution: read openspec/specs before edits, use openspec/changes for behavior deltas, and keep implementation aligned to As-Is specs.

- `diagnose/`
  - Disciplined debug loop for parser bugs, runtime failures, and regressions with deterministic fixture-first evidence.

- `tdd/`
  - Behavior-first, vertical-slice development workflow linked to OpenSpec capability IDs.

- `to-issues-openspec/`
  - Converts plans into implementation issues with explicit OpenSpec capability linkage.

- `zoom-out/`
  - Produces high-level system maps for onboarding and unfamiliar code areas.

- `grill-with-docs-openspec/`
  - Requirement clarification workflow for larger changes, grounded in OpenSpec and repo constraints.

- `proc-fragmentation-compat/`
  - Handles pagetypeinfo/buddyinfo parsing with fallback and format-variance tolerance.

- `report-schema-compat/`
  - Protects CSV/JSON output compatibility and documents schema-impacting updates.

## Usage guidance

- Use skills for specialized workflows, not general coding.
- Keep skill descriptions explicit so they are discovered reliably.
- Keep each skill focused on one domain concern.

## Related guide

- `.github/AGENTS_AND_SKILLS_USAGE.md`
  - Recommended invocation order and evidence checklist.
