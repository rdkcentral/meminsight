## MODIFIED Requirements

### Requirement: JSON structure

Current JSON report includes:

- top-level run metadata fields
- meminfo object
- cpustat object
- optional fragmentation object when --frag
- processes array with per-process rows
- synthetic total row appended in processes array

#### Scenario: JSON output includes cpustat object
- **WHEN** output format is JSON and cJSON is loaded successfully
- **THEN** the JSON root object SHALL contain a `"cpustat"` object with numeric fields for system-wide CPU statistics, placed after the meminfo object
