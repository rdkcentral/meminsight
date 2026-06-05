## ADDED Requirements

### Requirement: System-wide CPU stats extraction from /proc/stat

The system SHALL read the first (aggregate) `cpu` line from `/proc/stat` on every collection iteration and parse the following 8 fields (in order): user, nice, system, idle, iowait, irq, softirq, steal. Values are unsigned long representing cumulative clock ticks since boot.

#### Scenario: Successful parse of all 8 fields
- **WHEN** `/proc/stat` is readable and the first line begins with `cpu `
- **THEN** the system SHALL parse 8 unsigned long values from that line in positional order (user, nice, system, idle, iowait, irq, softirq, steal)

#### Scenario: Fewer than 8 fields available (older kernel)
- **WHEN** the aggregate `cpu` line contains fewer than 8 numeric fields
- **THEN** the system SHALL zero-fill any missing trailing fields and log a warning indicating the number of fields actually parsed

#### Scenario: /proc/stat is unreadable
- **WHEN** `/proc/stat` cannot be opened for reading
- **THEN** the system SHALL log an error, skip the CPU stats section in output, and continue the collection iteration without termination

### Requirement: CPU stats section in CSV output

The system SHALL emit a `/proc/stat:` CSV section containing a header row and a value row with the 8 CPU fields.

#### Scenario: CSV format output with CPU stats
- **WHEN** output format is CSV (default)
- **THEN** the output SHALL contain a section beginning with the line `/proc/stat:` followed by a header row `user,nice,system,idle,iowait,irq,softirq,steal` and a value row with the corresponding parsed values separated by commas

#### Scenario: Section placement in CSV
- **WHEN** the CSV report is being written
- **THEN** the `/proc/stat:` section SHALL appear after the `/proc/meminfo:` section and before the optional fragmentation section

### Requirement: CPU stats in JSON output

The system SHALL emit a `cpustat` object in the JSON report containing the 8 CPU fields as numeric values.

#### Scenario: JSON format output with CPU stats
- **WHEN** output format is JSON and cJSON is loaded
- **THEN** the JSON root object SHALL contain a `"cpustat"` object with keys: `"user"`, `"nice"`, `"system"`, `"idle"`, `"iowait"`, `"irq"`, `"softirq"`, `"steal"` and unsigned long numeric values

#### Scenario: JSON fallback when cJSON unavailable
- **WHEN** output format is JSON but cJSON loading fails
- **THEN** the system SHALL fall back to CSV output which includes the `/proc/stat:` section as specified above
