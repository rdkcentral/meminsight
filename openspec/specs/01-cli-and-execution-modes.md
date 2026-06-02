# C01 - Command-line interface and execution mode resolution (As-Is)

## Scope

This specification defines the current command-line behavior implemented by meminsight, including option validation, mode selection, and default run behavior.

## Inputs

Supported options in the current implementation:

- `-a, --all`
- `-c, --config <file>`
- `-o, --output <directory>`
- `--interval <seconds>`
- `--iterations <count>`
- `--upload-enable`
- `--upload-interval <seconds>`
- `--frag`
- `-s, --smaps`
- `-h, --help`
- `--fmt csv|json`
- `--json-pretty`
- `-t, --test` (only when TESTME build is enabled)

## Current behavior

1. No arguments selects system-wide capture mode.
2. `--config` selects config-driven whitelist capture mode.
3. `--help` prints detailed usage and exits.
4. Unrecognized arguments trigger usage error output and non-success exit.
5. `--json-pretty` is valid only when `--fmt json` is set.
6. `--upload-interval` is valid only when `--upload-enable` is set.
7. `--smaps` forces smaps parsing and disables smaps_rollup auto-selection.
8. Without forced smaps, the tool auto-selects smaps_rollup when available, otherwise smaps.
9. Long-run mode is active by default when no explicit interval/iterations pair is supplied.

## Defaults and precedence

1. CLI values override config file values.
2. Config file values override built-in defaults.
3. Built-in defaults include CSV output, standard output directory, bounded or long-run iteration behavior depending on flags, and disabled fragmentation collection unless requested.

## Source anchors

main(), printHelpAndUsage() in src/meminsight.c
