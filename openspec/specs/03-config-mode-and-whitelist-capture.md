# C03 - Config file parsing and whitelist-based capture mode (As-Is)

## Scope

This specification defines the configuration file contract and whitelist-driven capture behavior.

## Config format

1. Config path must use .conf extension.
2. Supported keys:
- process_whitelist
- output_dir
- iterations
- interval
- log_level

## Whitelist semantics

1. process_whitelist accepts comma-separated tokens.
2. Each token is interpreted as either a PID or a process name.
3. Process names are resolved to PID via proc enumeration of comm entries.
4. Missing process names are skipped with error logging and do not abort the entire run.

## Runtime precedence

1. CLI output directory overrides config output_dir.
2. CLI iterations and interval values override config values when valid.
3. Invalid or missing iteration/interval values are normalized to built-in defaults.

## Output behavior in config mode

1. Iterations emit per-iteration report files with run metadata.
2. Process totals are emitted after whitelisted process rows.
3. Meminfo, optional fragmentation, and optional bandwidth sections are written according to active flags and source availability.

## Source anchors

parseConfig(), getPIDByProcessName(), handleConfigMode() in src/meminsight.c
