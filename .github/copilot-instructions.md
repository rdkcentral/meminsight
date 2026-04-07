# MemInsight Workspace Instructions

## Engineering ideology

- Prefer correctness and parser tolerance over aggressive assumptions.
- Do not invent kernel-field schemas; parse defensively and preserve backward compatibility.
- CSV output compatibility is mandatory; avoid breaking existing field order/section structure unless explicitly requested.
- JSON is optional and must gracefully degrade to CSV when unavailable.
- Keep changes minimal, predictable, and memory-safe for embedded targets.

## Implementation rules

- Keep helper functions file-local unless cross-file reuse is needed.
- Always check file I/O return values and fail soft for optional data sources.
- For `/proc` parsing, support missing files and format drift without crashing.
- Preserve current CLI behavior unless the request explicitly changes it.
- Under `TESTME`, prefer deterministic fixture-driven validation.

## Testing rules

- For parser changes, add or update fixture-based tests in `tst/` and `scripts/run_ut.sh`.
- Validate both positive and negative paths.
- For optional features (`--enable-cjson`, fragmentation inputs), include fallback-path checks.

## Build/CI rules

- Use `cov_build.sh --clean` before matrix-like verification runs.
- Use explicit feature flags (`--enable-cjson`, `--test`) and avoid implicit behavior.
- Keep workflow jobs single-purpose (build-only vs test-only).
