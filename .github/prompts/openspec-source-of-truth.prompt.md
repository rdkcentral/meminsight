# OpenSpec Source-of-Truth Prompt

Use this prompt for any implementation, bug fix, refactor, or review task in meminsight.

## Instructions

1. Start by reading impacted capability files under openspec/specs.
2. Treat openspec/specs as the authoritative requirements source.
3. If behavior changes are needed, write or update entries in openspec/changes before code changes.
4. Keep implementation updates and spec updates synchronized for touched capabilities.
5. Reject speculative requirements in baseline specs and document only verified As-Is behavior.
6. Report final changes using capability IDs and corresponding spec file names.
