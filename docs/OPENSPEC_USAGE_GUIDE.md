# OpenSpec Usage Guide for meminsight

## Purpose

OpenSpec is the requirements source-of-truth for meminsight behavior and change management.

## Directory structure

- openspec/config.yaml: project context, stack metadata, and agent behavior rules.
- openspec/specs/: As-Is capability specifications.
- openspec/changes/: delta specs for intentional behavior changes.

## How to use OpenSpec

### Quick navigation steps

1. Open openspec/specs/00-capability-index.md.
2. Find the capability IDs related to your change.
3. Read detailed capability files under openspec/specs/.
4. Use openspec/specs/13-implementation-parity-matrix.md to locate implementation anchors.

### 1. For bug fixes with no behavior change

1. Identify impacted capability file(s) in openspec/specs.
2. Implement fix while preserving documented behavior.
3. Update tests/fixtures to prove regression protection.
4. Do not add delta specs in openspec/changes for pure bug-only conformance fixes.
5. Verify reviewer checks pass for spec conformance and compatibility.

### 2. For intentional behavior changes

1. Add a delta specification in openspec/changes first.
2. Reference impacted capability IDs from openspec/specs.
3. Implement and validate code changes.
4. Update capability specs when the new behavior becomes baseline.
5. Clear active delta specs only after approval and merge.

### 2a. Delta spec authoring checklist

1. Problem statement and current behavior.
2. Intended behavior and acceptance criteria.
3. Impacted capability IDs and files.
4. Validation approach (tests, fixtures, commands).
5. Rollout and compatibility notes if applicable.

### 3. For reviews

1. Verify implementation aligns with capability files.
2. Verify any behavior drift is justified by a change spec.
3. Verify tests demonstrate acceptance criteria.
4. Verify documentation updates are included when behavior changed.

### 4. Baseline promotion steps

1. Confirm behavior change is approved and merged.
2. Update openspec/specs to reflect new As-Is behavior.
3. Update parity matrix if implementation anchors changed.
4. Remove or archive completed delta specs from openspec/changes.
5. Reconfirm openspec/changes baseline state.

## Configuration usage

The openspec/config.yaml file defines:

1. Repository context and quality principles.
2. Technical stack and validation commands.
3. Agent operational protocol for OpenSpec-first execution.
4. Governance constraints for baseline and documentation policy.

## Capability workflow

1. Start from openspec/specs/00-capability-index.md.
2. Locate detailed capability file(s) for impacted behavior.
3. Use openspec/specs/13-implementation-parity-matrix.md for implementation anchors.

## Baseline rules

1. openspec/specs must document verified As-Is behavior only.
2. Avoid speculative wording.
3. openspec/changes represents active behavior deltas and should remain empty when baseline is stable.

## Where this guide fits

1. Root README: quick links and orientation only.
2. docs/: detailed operational guidance.
3. openspec/: normative behavior and change artifacts.
