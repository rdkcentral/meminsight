## MODIFIED Requirements

### Requirement: CSV section order

Current section order:

1. Metadata header row.
2. Metadata value row.
3. Meminfo section.
4. CPU stats section.
5. Optional fragmentation section (only when `--frag`).
6. Processes section header and per-process rows sorted by pss descending.
7. Synthetic total row with pid 0 and name Total.
8. Optional bandwidth section when available.

#### Scenario: Section ordering with CPU stats
- **WHEN** a CSV report is written
- **THEN** the `/proc/stat:` section SHALL appear immediately after the `/proc/meminfo:` section and before the optional fragmentation section
