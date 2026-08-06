# Documentation Map

This directory contains detailed operational and governance documentation.

## Guides

- OPENSPEC_USAGE_GUIDE.md
  - Step-by-step OpenSpec workflow for fixes, behavior changes, reviews, architecture intake, and baseline promotion.

- ROLE_BASED_WORKFLOW_GUIDE.md
  - Responsibilities and workflows for reviewer, developer, architect owner, tester, and technical documentation expert, including opsx lifecycle usage.

## Documentation placement policy

1. Root README: concise project overview and entry links.
2. docs/: detailed process and role guidance.
3. openspec/: normative capability and change documentation.
4. .github/: agent, skill, and prompt execution assets.

## Related references

1. [openspec/architecture/README.md](../openspec/architecture/README.md)
2. [.github/AGENTS_AND_SKILLS_USAGE.md](../.github/AGENTS_AND_SKILLS_USAGE.md)
3. [.github/prompts/README.md](../.github/prompts/README.md)

## Backup and metadata specs

1. [openspec/specs/01-cli-and-execution-modes.md](../openspec/specs/01-cli-and-execution-modes.md) documents `-b/--backup` and range validation.
2. [openspec/specs/02-output-directory-and-run-lifecycle.md](../openspec/specs/02-output-directory-and-run-lifecycle.md) documents pre-run backup behavior.
3. [openspec/specs/10-json-report-output-optional.md](../openspec/specs/10-json-report-output-optional.md) documents JSON backup metadata fields.

## Recommended reading order for new contributors

1. [README.md](../README.md)
2. [openspec/architecture/00-baseline-architecture.md](../openspec/architecture/00-baseline-architecture.md)
3. [docs/OPENSPEC_USAGE_GUIDE.md](OPENSPEC_USAGE_GUIDE.md)
4. [docs/ROLE_BASED_WORKFLOW_GUIDE.md](ROLE_BASED_WORKFLOW_GUIDE.md)
5. [.github/AGENTS_AND_SKILLS_USAGE.md](../.github/AGENTS_AND_SKILLS_USAGE.md)
