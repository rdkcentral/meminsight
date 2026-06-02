# C12 - TESTME fixture mode and parser regression checks (As-Is)

## Scope

This specification defines test-only runtime behavior enabled by TESTME builds.

## Entry behavior

1. Test mode is enabled via -t or --test in TESTME builds.
2. Required fixture inputs are smaps fixture and meminfo fixture.
3. Optional fixtures are buddyinfo and pagetypeinfo.

## Current validation behavior

1. Parser outputs derived from runtime parsing are compared against fixture-driven parser expectations for selected fields.
2. Mismatch conditions mark unit test failure state.
3. In test flow, iteration count is constrained according to current test-mode control logic.

## Fixture usage

1. Meminfo and map parsing functions read fixture paths when test mode is active.
2. Fragmentation source selection in test mode is inferred from which optional fixture paths are supplied.

## Source anchors

main(), testGetProcessInfos_Parse(), testGetProcessInfos(), getProcessInfos(), saveMeminfo(), selectFragmentationSource(), collectSystemMemoryStats() in src/meminsight.c
