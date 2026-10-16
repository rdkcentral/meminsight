# C03 - Config file parsing and whitelist-based capture mode (As-Is)

## Scope

This specification defines the configuration file contract and whitelist-driven capture behavior.

## Config format

1. Config path must use .conf extension.
2. Config path must not contain a `..` path component.
3. Supported keys:
- process_whitelist
- output_dir
- iterations
- interval
- log_level
4. `output_dir` from the config file is subject to the same rules as `--output`: no `..` component, and the final path component must contain `meminsight`.
5. `iterations` from the config file is sanitized to the range 1..100000. A message is printed when the parsed value is changed.

## Whitelist semantics

1. process_whitelist accepts comma-separated tokens.
2. Each token is interpreted as either a PID or a process name.
3. Process names are resolved to PID via proc enumeration of comm entries.
4. Missing process names are skipped with error logging and do not abort the entire run.

## Runtime precedence

1. CLI output directory overrides config output_dir.
2. An explicit CLI `--iterations` value is sanitized first, then overrides the config value. `--iterations 0` becomes 1 and still overrides a config of 10.
3. Invalid or missing iteration/interval values are normalized to built-in defaults. Iteration counts above 100000 are capped; values below 1 fall back to the default. A message is printed when the iteration count is changed.

## Output behavior in config mode

1. Iterations emit per-iteration report files with run metadata.
2. Process totals are emitted after whitelisted process rows.
3. Meminfo, optional fragmentation, and optional bandwidth sections are written according to active flags and source availability.

## Source anchors

parseConfig(), getPIDByProcessName(), handleConfigMode() in src/meminsight.c
