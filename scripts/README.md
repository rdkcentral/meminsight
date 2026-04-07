# MemInsight Build Scripts

This directory contains organized, modular build and test scripts for meminsight development and CI/CD operations.

## Script Organization

### Build & Compilation

- **`build_memleak.sh`** - Clone and build memleakutil library
  - Handles git operations, autotools configuration, and parallel compilation
  - Produces `libmemfnswrap.so` for memory leak detection
  - **Usage**: `sh scripts/build_memleak.sh [WORK_DIR] [GIT_BRANCH]`
  - **Output**: Installation to `/tmp/memleakutil-install` (configurable)

### Testing & Analysis

- **`detect_leak.sh`** - Run meminsight with memory leak detection
  - Instruments meminsight via `LD_PRELOAD` with libmemfnswrap.so
  - Generates leak detection reports and heap walk summaries
  - **Usage**: `sh scripts/detect_leak.sh -o /tmp/out -i 1 -I 0`
  - **Output**: Leak reports to `/tmp/meminsight-leak-reports` (configurable)

### Root-Level Scripts (Retained for compatibility)

- **`cov_build.sh`** - Main autotools build wrapper (kept at project root)
  - Handles clean, configure, build steps for meminsight
  - Supports feature flags (`--enable-cjson`, `--test`)
  - **Usage**: `sh cov_build.sh --enable-cjson --test`

- **`scripts/run_ut.sh`** - Unit test runner
  - Executes fragmentation, memory, and negative tests
  - Uses test fixtures from `tst/` directory
  - **Usage**: `sh scripts/run_ut.sh`

## Complete Build Workflow

### 1. Build MemInsight

```bash
# Clean and build with cJSON and test mode enabled
sh cov_build.sh --clean
sh cov_build.sh --enable-cjson --test
```

### 2. Build MemLeakUtil (for leak detection)

```bash
# Clone, configure, build, and install memleakutil
sh scripts/build_memleak.sh
```

### 3. Run Unit Tests

```bash
# Execute all meminsight unit tests
sh scripts/run_ut.sh
```

### 4. Run Leak Detection Analysis

```bash
# Run meminsight with memory leak instrumentation
sh scripts/detect_leak.sh -o /tmp/meminsight-output -i 1 -I 0
```

### 5. Full CI/CD Pipeline

```bash
# Complete pipeline: build, test, leak detection
sh cov_build.sh --clean && \
sh cov_build.sh --enable-cjson --test && \
sh scripts/run_ut.sh && \
sh scripts/build_memleak.sh && \
sh scripts/detect_leak.sh -o /tmp/output -i 1 -I 0
```

## Environment Variables

### build_memleak.sh

| Variable | Default | Purpose |
|----------|---------|---------|
| `MEMLEAK_INSTALL_DIR` | `/tmp/memleakutil-install` | Installation prefix for libmemfnswrap.so |
| `MEMLEAK_JOBS` | `4` | Parallel build jobs for make |
| `MEMLEAK_GIT_URL` | GitHub URL | Git repository to clone from |
| `MEMLEAK_GIT_BRANCH` | `main` | Git branch to build |

### detect_leak.sh

| Variable | Default | Purpose |
|----------|---------|---------|
| `MEMLEAK_INSTALL_DIR` | `/tmp/memleakutil-install` | Path to memleakutil installation |
| `MEMINSIGHT_BIN` | `./meminsight` | Path to built meminsight binary |
| `LEAK_REPORT_DIR` | `/tmp/meminsight-leak-reports` | Output directory for leak reports |

## CI/CD Integration

### GitHub Actions - native_full_build.yml

The workflow automatically orchestrates:

1. Checkout code
2. Install build dependencies
3. Clone and build memleakutil
4. Build meminsight with cJSON and test features
5. Run meminsight with leak detection instrumentation
6. Generate and attach leak detection reports

### Example Usage in Workflows

```yaml
- name: Build memleakutil
  run: sh scripts/build_memleak.sh /tmp

- name: Run leak detection
  run: sh scripts/detect_leak.sh -o /tmp/out -i 1
```

## Development Best Practices

### Adding New Scripts

1. Place in `scripts/` directory
2. Use `.sh` extension with shebang `#!/bin/sh` unless bash-only features are required
3. Include help documentation (`--help` flag)
4. Add colored output for readability
5. Implement proper error handling (`set -e`)
6. Document in this README

### Error Handling Patterns

```bash
# Check for required files
if [ ! -f "${REQUIRED_FILE}" ]; then
    echo -e "${RED}ERROR: ${REQUIRED_FILE} not found${NC}"
    exit 1
fi

# Run with error propagation
set -e
some_command
another_command

# Optional error recovery
set +e
risky_command_that_may_fail
EXIT_CODE=$?
set -e
```

### Output Formatting

Use color codes for consistency:

```bash
RED='\033[0;31m'        # Errors
YELLOW='\033[1;33m'     # Warnings
GREEN='\033[0;32m'      # Success
BLUE='\033[0;34m'       # Headers
CYAN='\033[0;36m'       # Info
NC='\033[0m'            # No Color

echo -e "${RED}ERROR: message${NC}"
echo -e "${GREEN}✓ Success${NC}"
```

## Testing Scripts Locally

```bash
# Test build_memleak.sh
sh scripts/build_memleak.sh /tmp main

# Test detect_leak.sh (requires prior meminsight build)
sh cov_build.sh --enable-cjson --test
sh scripts/detect_leak.sh -o /tmp/test-output -i 1

# Test with custom paths
MEMLEAK_INSTALL_DIR=/custom/path sh scripts/detect_leak.sh -o /tmp/out -i 1
```

## Troubleshooting

### build_memleak.sh

| Issue | Solution |
|-------|----------|
| `git: command not found` | Install git: `apt-get install git` |
| `autoconf: not found` | Install autotools: `apt-get install autotools-dev autoconf` |
| Permission denied | Make executable: `chmod +x scripts/build_memleak.sh` |
| Build fails | Check Docker image and run: `apt-get update && apt-get install -y build-essential` |

### detect_leak.sh

| Issue | Solution |
|-------|----------|
| `libmemfnswrap.so not found` | Build first: `sh scripts/build_memleak.sh` |
| `meminsight binary not found` | Build first: `sh cov_build.sh --enable-cjson --test` |
| No leak reports generated | Check `LEAK_REPORT_DIR` permissions and disk space |

## Maintenance

### Version Updates

When updating tool versions:

1. Update version strings in script comments
2. Update README with new features
3. Test in CI/CD pipeline
4. Document breaking changes

### Script Dependencies

- **build_memleak.sh**: git, autoconf, automake, make, Python
- **detect_leak.sh**: bash, memleakutil (built via build_memleak.sh), libdl
- **cov_build.sh**: autoconf, automake, gcc, make
- **scripts/run_ut.sh**: standard POSIX utilities

---

**Last Updated**: April 7, 2026  
**Maintainer**: MemInsight Team  
**CI/CD**: GitHub Actions Integration Active
