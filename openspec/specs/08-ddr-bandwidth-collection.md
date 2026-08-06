# C08 - DDR bandwidth section collection (As-Is)

## Scope

This specification defines optional DDR bandwidth reporting behavior based on sysfs accessibility checks.

## Data sources

- Mode control path: /sys/class/aml_ddr/mode
- Bandwidth data path: /sys/class/aml_ddr/bandwidth

## Current behavior

1. Availability is determined at startup by checking required file access permissions.
2. Bandwidth collection is attempted only when startup checks indicate availability.
3. During collection, mode is verified and enabled when required.
4. When a parseable line is read, CSV output appends a Bandwidth section with total bandwidth and usage percentage.
5. When JSON output is active and a parseable line is read, JSON output includes a `bandwidth` object with total bandwidth and usage percentage.
6. Failures to open or parse bandwidth sources are logged and do not terminate the run.

## Source anchors

updateBandwidthAvailability(), readBandwidthData(), collectBandwidthData(), collectBandwidthData_JSON() in src/meminsight.c
