# 🔍 meminsight

**meminsight** is a powerful, lightweight Linux memory profiling and monitoring tool designed for system administrators, developers, and embedded system engineers. It provides comprehensive real-time memory usage analysis with detailed per-process statistics and system-wide memory insights.

---

## 📋 Table of Contents

- [Features](#-features)
- [Quick Start](#-quick-start)
- [Development Setup](#development-setup)
- [Installation](#-installation)
- [Usage](#-usage)
- [Configuration](#-configuration)
- [Advanced Features](#-advanced-features)
- [Build System](#-build-system)
- [Architecture](#-architecture)
- [OpenSpec Workflow](#-openspec-workflow)
- [Role-Based Workflow](#-role-based-workflow)
- [Agents and Skills](#-agents-and-skills)
- [Report Metadata](#-report-metadata)
- [Examples](#-examples)
- [Troubleshooting](#-troubleshooting)
- [Contributing](#-contributing)

## ✨ Features

### 🎯 **Comprehensive Memory Analysis**
- **RSS (Resident Set Size)** - Physical memory currently used by processes
- **PSS (Proportional Set Size)** - RSS + a portion of shared memory
- **Shared Clean Memory** - Read-only shared pages
- **Private Dirty Memory** - Process-exclusive modified pages
- **Swap PSS** - Proportional swap usage
- **Major Page Faults** - Hard page faults requiring disk I/O
- **CPU Time** - Combined user and system CPU usage

### 🛠️ **Flexible Configuration**
- **Process Whitelisting** - Monitor specific processes by name or PID
- **Configurable Sampling** - Set iterations and intervals for data collection
- **Multiple Output Formats** - CSV and JSON export with timestamps and metadata
- **Kernel Thread Support** - Optional inclusion of kernel threads
- **Long-running Mode** - Extended monitoring with automatic intervals
- **Fragmentation Data Capture** - Optional via `--frag`; `/proc/pagetypeinfo` preferred with `/proc/buddyinfo` fallback

### 🔧 **Advanced Capabilities**
- **Network Interface Detection** - Automatic MAC address retrieval
- **Firmware Information** - System version and device property extraction
- **Memory Leak Detection** - Track memory growth over time
- **Performance Profiling** - CPU and memory correlation analysis
- **Embedded System Optimized** - Minimal overhead for resource-constrained environments

## 🚀 Quick Start

```bash
# Build the tool
./configure && make

# Run basic system-wide memory analysis
./meminsight

# Enable JSON output (requires --enable-cjson at build time)
./meminsight --fmt json --json-pretty --iterations 2 --interval 10

# Run a finite capture (overrides defaults)
./meminsight --iterations 10 --interval 30

# Include kernel threads in analysis
./meminsight --all

# Write reports to a custom directory
./meminsight --output /tmp/memreports --iterations 3

# Enable fragmentation data collection (disabled by default)
./meminsight --frag --iterations 5 --interval 60
```

## 🐳 Docker

The repository includes a multi-stage `Dockerfile` with dedicated targets for build, test, and runtime.

```bash
# Build runtime image
docker build -t meminsight:latest .

# Run fixture tests inside the image build
docker build --target test -t meminsight:test .

# Run meminsight help
docker run --rm meminsight:latest --help

# Example finite run (writes reports inside container)
docker run --rm meminsight:latest --iterations 1 --interval 0
```

## 📊 Build & Testing Scripts

MemInsight includes organized build and testing infrastructure in the `scripts/` directory:

### Quick Build & Test

```bash
# Full build pipeline: compile, test, memory leak detection
sh cov_build.sh --clean && \
sh cov_build.sh --enable-cjson --test && \
sh test/run_ut.sh && \
sh scripts/build_memleak.sh && \
sh scripts/detect_leak.sh -o /tmp/output -i 1 -I 2 --json-pretty
```

### Build & Memory Leak Detection

```bash
# 1. Build meminsight with test features
sh cov_build.sh --enable-cjson --test

# 2. Build memleakutil (memory leak detection library)
sh scripts/build_memleak.sh

# 3. Run meminsight with leak detection instrumentation
sh scripts/detect_leak.sh -o /tmp/output -i 1 -I 2 --json-pretty

# 4. View leak detection report
cat /tmp/meminsight-leak-reports/leak_report_*.txt
```

### Available Scripts

| Script | Purpose |
|--------|---------|
| `scripts/build_memleak.sh` | Clone and build memleakutil library for memory profiling |
| `scripts/detect_leak.sh` | Run meminsight with LD_PRELOAD instrumentation for leak detection |
| `scripts/README.md` | Complete documentation and troubleshooting guide |
| `cov_build.sh` | Main autotools build wrapper (root-level for compatibility) |
| `test/run_ut.sh` | Unit test runner for all test fixtures |

**See [scripts/README.md](scripts/README.md) for complete documentation and environment variables.**

## 📋 Code Audit & Hardening

A comprehensive code audit has been performed identifying memory safety, resource management, and optimization improvements:

Audit outcomes currently tracked in repository documentation and planning notes include:
- **3 CRITICAL resource leaks** (file handles, directory handles) with fixes
- **7 High/Medium issues** (overflow risks, crash possibilities, missing checks)
- **5 Low-severity optimizations** (code structure, performance)
- **Remediation roadmap** with effort estimates
- **Safe C programming patterns** and best practices

**Status**: 
- ✅ Code audit and findings complete
- ✅ CRITICAL resource leak fixes completed
- ℹ️ Integrated memleakutil for runtime leak detection in CI/CD

## Development Setup

```bash
# Setup development environment
autoreconf -fiv
./configure --enable-test
make

# Run test fixtures (requires --enable-test / TESTME build)
sh test/run_ut.sh

# Equivalent wrapper flow
sh cov_build.sh --test
sh test/run_ut.sh
```

## 🔨 Installation

### Prerequisites
- Linux operating system (kernel 2.6+)
- GCC compiler
- GNU Autotools (autoconf, automake)
- Standard C library with POSIX support

### Building from Source

```bash
# Clone or extract the source code
cd meminsight/

# Generate configure script
autoreconf -fiv

# Configure build environment
./configure

# Compile the binary
make

# Optional: Install system-wide
sudo make install
```

## 📖 Usage

### Command Line Options

```bash
meminsight [OPTIONS]

OPTIONS:
   -a, --all                   Include kernel threads for process monitoring
   -c, --config FILE           Path to configuration file (must end with .conf)
   -o, --output DIR            Output directory for generated reports (default: /opt/meminsight)
         --iterations N          Number of iterations to run (overrides config)
         --interval SECONDS      Seconds between iterations (overrides config)
      --fmt FORMAT            Report format: csv (default) or json
      --json-pretty           Pretty-print JSON output (only with --fmt json)
      --frag                  Enable fragmentation data collection (default: disabled)
      --upload-enable         Enable upload infrastructure (creates marker file)
      --upload-interval SECS  Upload cadence in seconds (requires --upload-enable)
   -t, --test SMAPS MEMINFO [BUDDYINFO] [PAGETYPEINFO]
                               Run in test mode using supplied sample files (requires TESTME build)
   -h, --help                  Show help message and exit
```

### Default Behavior

If you run `./meminsight` with no flags, it runs in a long-running mode (indefinite iterations) with a 15-minute interval and writes CSV reports under `/opt/meminsight/`.

To run a finite capture, specify `--iterations` and/or `--interval`.

### Basic Usage Examples

```bash
# Single snapshot of system memory
./meminsight --iterations 1 --interval 0

# Monitor for 1 hour with 5-minute intervals
./meminsight --iterations 12 --interval 300

# Continuous monitoring with kernel threads
./meminsight --all
```

### Upload Infrastructure

The tool supports automatic report upload signaling via systemd path-triggered units:

```bash
# Enable upload marker creation
./meminsight --upload-enable --iterations 5 --interval 60

# Enable upload with cadence (upload script decides upload frequency)
./meminsight --upload-enable --upload-interval 3600 --iterations 10 --interval 300
```

When `--upload-enable` is passed:
- A marker file `/tmp/.meminsight_upload` is created immediately before the capture run begins
- A state file `/tmp/.meminsight_configstore` is written with run parameters
- An in-progress sentinel `/tmp/.meminsight_inprogress` is created at run start and removed at completion
- The systemd `meminsight-upload.path` unit watches for the marker and triggers the upload service

## 📁 State Files

Meminsight creates and manages the following state files:

### `/tmp/.meminsight_configstore` (Persistent, Per-Run)

**Purpose**: Store run parameters for the upload script to read.

**Format**: Key=value pairs (one per line).

**Contents**:
```
UPTIME=12345.67
KERNEL_VERSION=5.15.0-91-generic
MEMINSIGHT_VERSION=1.1.0
REPORT_VERSION=1.1.0
RUN_ITERATIONS=10
RUN_INTERVAL=60
RUN_ID=17014563271234507
OUTPUT_FORMAT=csv
UPLOAD_ENABLED=1
UPLOAD_INTERVAL=3600
OUTPUT_DIR=/opt/meminsight
```

**Behavior**:
- Written before each capture run starts
- Read first; if every key already matches current values, file is left untouched
- If any value changes or file is absent, entire file is atomically rewritten
- Never deleted by meminsight (persists as historical record)

### `/tmp/.meminsight_inprogress` (Temporary, Per-Run)

**Purpose**: Signal that a capture run is actively in progress.

**Behavior**:
- Created immediately before the capture loop begins
- Removed when the run completes (success or error path)
- Allows external tools to detect incomplete or stalled runs

This path is defined in code as `MEMINSIGHT_INPROGRESS_FILE`.

### `/tmp/.meminsight_upload` (Temporary, Upload Trigger)

**Purpose**: Trigger the systemd path-activated upload service.

**Behavior**:
- Created immediately before the capture run begins (if `--upload-enable` is passed)
- Watched by `meminsight-upload.path` systemd unit
- When detected, systemd triggers `meminsight-upload.service` to run the upload script

## ⚙️ Configuration

### Configuration File Format

Create a `.conf` file with the following parameters:

```ini
# production.conf
process_whitelist=nginx,mysql,redis,1234
output_dir=/var/log/meminsight
iterations=24
interval=3600
log_level=INFO
```

### Configuration Parameters

| Parameter | Description | Default Value | Example |
|-----------|-------------|---------------|---------|
| `process_whitelist` | Comma-separated list of process names/PIDs | All processes | `apache2,mysql,1234` |
| `output_dir` | Directory for output files | `/opt/meminsight` | `/var/log/monitoring` |
| `iterations` | Number of sampling cycles | `1` | `10` |
| `interval` | Seconds between samples | `5` | `60` |
| `log_level` | Logging verbosity (parsed from config; current builds may not print logs unless debug is enabled) | `INFO` | `DEBUG`, `ERROR` |

### JSON and Fragmentation Output

- Build with JSON support:

```bash
./configure --enable-cjson
make
```

- Run with JSON output:

```bash
./meminsight --fmt json --json-pretty --iterations 2 --interval 30
```

- Fragmentation collection is **disabled by default** and must be explicitly enabled with `--frag`:

```bash
./meminsight --frag --iterations 5 --interval 60
```

  When `--frag` is active, collection behavior is:
   - If `/proc/pagetypeinfo` is readable, collect and emit pagetype data.
   - Else if `/proc/buddyinfo` is readable, collect and emit buddyinfo data.
   - Else, skip fragmentation section for that iteration without failure.

  When `--frag` is **not** passed, the fragmentation section is omitted from all reports and no `/proc` fragmentation files are read.

### Network Interface Configuration

The tool automatically detects network interfaces for MAC address collection:

```bash
# Default behavior:
# - Reads /etc/device.properties using DEVICE_INTERFACE_KEY (default: ESTB_INTERFACE)
# - Uses the property value as the interface name for MAC lookup
# - Falls back to DEFAULT_MAC (000000000000) if file/key/value is missing or MAC lookup fails
./meminsight

# Custom device-properties key via compile-time flag
CPPFLAGS="-DDEVICE_INTERFACE_KEY=\"ETH_INTERFACE\"" make clean && make

# Example /etc/device.properties entry when using default key
echo "ESTB_INTERFACE=erouter0" >> /etc/device.properties
```

## 🔬 Advanced Features

### Upload Architecture

Meminsight integrates with systemd path-triggered units for autonomous report uploads:

**Systemd Units**:
- `meminsight-upload.path` — Watches `/tmp/.meminsight_upload` and triggers the service on file creation/modification
- `meminsight-upload.service` — Runs `/usr/bin/upload_MemReports.sh` with idle scheduling (Nice=15, IOSchedulingClass=idle)

**Workflow**:

1. **Run with upload enabled**:
   ```bash
   ./meminsight --upload-enable --upload-interval 3600 --iterations 48 --interval 600
   ```

2. **Before capture**:
   - Configstore written with all run parameters
   - Upload marker `/tmp/.meminsight_upload` created
   - In-progress sentinel created in output directory

3. **Systemd detects marker**:
   - `meminsight-upload.path` detects `/tmp/.meminsight_upload`
   - Triggers `meminsight-upload.service`

4. **Upload script runs**:
   - Reads configstore to determine upload parameters and report location
   - Collects reports using `RUN_ID` for correlation
   - Implements cadence-based uploads (e.g., every `UPLOAD_INTERVAL` seconds)

5. **After capture**:
   - In-progress sentinel removed
   - Configstore persists for historical reference
   - Upload script may clean up the marker file

### Memory Leak Detection

```bash
# Monitor memory growth over 8 hours
./meminsight --iterations 48 --interval 600 --config leak_detection.conf
```

### Process-Specific Monitoring

```bash
# Monitor only critical services
echo "process_whitelist=systemd,NetworkManager,sshd" > services.conf
./meminsight --config services.conf --iterations 60 --interval 60
```

### Test Mode for Validation

```bash
# Build with test-mode enabled (required for -t/--test)
make clean && make CFLAGS="-DTESTME"

# Run using sample fixtures (smaps + meminfo)
./meminsight --test test/1-non-zero-swap-entry/meminsight_testSmap.txt test/1-non-zero-swap-entry/meminsight_testMeminfo.txt

# Run using sample fixtures including fragmentation (buddyinfo + pagetypeinfo)
./meminsight --test test/1-non-zero-swap-entry/meminsight_testSmap.txt test/1-non-zero-swap-entry/meminsight_testMeminfo.txt test/6-buddyinfo-sample/meminsight_testBuddyinfo.txt test/7-pagetypeinfo-sample/meminsight_testPagetypeinfo.txt

# Run the repository unit-test runner (executes all fixtures and a negative test)
sh test/run_ut.sh
```

The `test/` directory contains per-test subdirectories (e.g. `test/1-non-zero-swap-entry/`) holding:
- `meminsight_testSmap.txt`
- `meminsight_testMeminfo.txt`

There is also a negative fixture in `test/4-negative-duplicate-meminfo-field/` that is expected to fail in test mode and emit:
`Test Failed..meminfoHeader vs tstmeminfoHeader ...`

Fragmentation fixture coverage is also included via:
- `test/6-buddyinfo-sample/meminsight_testBuddyinfo.txt`
- `test/7-pagetypeinfo-sample/meminsight_testPagetypeinfo.txt`

## 🏗️ Build System

### Autotools Configuration

The project uses GNU Autotools for cross-platform compatibility:

- **`configure.ac`** - Autoconf configuration
- **`Makefile.am`** - Automake build rules
- **Generated files** - `configure`, `Makefile.in`, `config.h`

### Build Targets

```bash
# Standard build
make

# Clean build artifacts
make clean

# Full cleanup including generated files
make distclean

# Create distribution tarball
make dist

# Install to system directories
make install
```

## 🏛️ Architecture

### Memory Collection Pipeline

1. **Process Discovery** - Scan `/proc` filesystem
2. **Statistics Parsing** - Read `/proc/[pid]/stat` and `/proc/[pid]/smaps`
3. **Data Aggregation** - Calculate totals and averages
4. **Sorting & Filtering** - Apply whitelist and sort by PSS
5. **Output Generation** - Write CSV with metadata

### Execution Flow (Manual and Automatic)

1. **Argument parsing** — Parse CLI options including output directory and upload flags.
2. **Startup sanitization** — `ensure_output_dir()` recursively wipes all contents of the output directory so each run starts clean. The directory itself is preserved or created if absent.
3. **Setup initialization** — Cache MAC address, firmware name, kernel version, and generate a per-run `RUN_ID` by concatenating epoch seconds + PID + a randomly generated 2-digit suffix.
4. **State file creation** — Write `/tmp/.meminsight_configstore` with resolved run parameters. This file persists across runs and is selectively updated.
5. **Upload marker creation** — If `--upload-enable` was passed, create `/tmp/.meminsight_upload` to signal the systemd upload service.
6. **In-progress sentinel** — Create `/tmp/.meminsight_inprogress` to mark an active run.
7. **Iteration loop** — For each iteration:
   - Capture fresh timestamp and uptime.
   - Collect system meminfo, process smaps stats.
   - If `--frag` is active, collect fragmentation data.
   - Write CSV/JSON report with full metadata row: `FIRMWARE_NAME, MAC_ADDRESS, TIMESTAMP, UPTIME, KERNEL_VERSION, REPORT_VERSION, ITERATION, RUN_ITERATIONS, RUN_INTERVAL, RUN_ID`.
8. **Cleanup** — Remove in-progress sentinel on completion or error. Configstore persists for upload script reference.
9. **Automatic run (systemd)** — Service starts meminsight with desired flags; path unit watches for marker and triggers upload service.

### Key Functions

| Function | Purpose | Location |
|----------|---------|----------|
| `collectSystemMemoryStats()` | Main collection orchestrator | meminsight.c |
| `getProcessInfos()` | Parse per-process smaps data | meminsight.c |
| `fillProcessStatFields()` | Extract stat file information | meminsight.c |
| `addProcessInfo()` | Maintain sorted process list | meminsight.c |
| `getMacAddress()` | Network interface detection | meminsight.c |
| `parseConfig()` | Configuration file processing | meminsight.c |
| `ensure_output_dir()` | Create output dir and wipe stale contents on startup | meminsight.c |
| `initializeSetupInfo()` | Cache device metadata and generate run hash | meminsight.c |
| `writeConfigStore()` | Write/update persistent state file to `/tmp/.meminsight_configstore` | meminsight.c |
| `touchFile()` | Create or truncate marker files | meminsight.c |
| `removeFileIfPresent()` | Gracefully remove in-progress sentinel on exit | meminsight.c |

---

## 💡 Examples

### Example 1: Web Server Monitoring

```bash
# Monitor web server processes
cat > webserver.conf << EOF
process_whitelist=nginx,apache2,httpd,php-fpm
output_dir=/var/log/webserver-memory
iterations=288
interval=300
log_level=INFO
EOF

./meminsight --config webserver.conf
```

### Example 2: Database Performance Analysis

```bash
# Monitor database memory usage every minute for 2 hours
./meminsight --iterations 120 --interval 60 \
              --output /tmp/db-analysis \
              --config database.conf
```

### Example 3: Embedded System Monitoring

```bash
# Lightweight monitoring for embedded systems
CPPFLAGS="-DDEVICE_INTERFACE_KEY=\"ESTB_INTERFACE\"" make clean && make
# Ensure /etc/device.properties contains: ESTB_INTERFACE=eth0
./meminsight --iterations 24 --interval 3600 --output /mnt/logs/
```

### Example 4: Upload-Enabled Monitoring (12-Hour Capture)

```bash
# Capture system memory every 15 minutes (48 iterations) with hourly upload cadence
./meminsight --upload-enable --upload-interval 3600 \
             --iterations 48 --interval 900 \
             --output /opt/meminsight-reports \
             --fmt json --json-pretty

# What happens:
# 1. Before any capture, configstore is written with run parameters
# 2. Upload marker /tmp/.meminsight_upload is created (triggers systemd service)
# 3. In-progress sentinel created at /tmp/.meminsight_inprogress
# 4. Systemd service detects marker and begins monitoring for reports
# 5. Every 15 minutes a JSON report is written with RUN_ID in the filename
# 6. Upload service reads configstore, finds UPLOAD_INTERVAL=3600, and uploads accordingly
# 7. After 48 iterations (12 hours), in-progress sentinel is removed
# 8. Configstore persists for audit and future reference
```

### Example 5: Config File with Upload Settings

```bash
# monitoring.conf
process_whitelist=systemd,nginx,mysql
output_dir=/var/log/meminsight
iterations=24
interval=300

# Run with upload enabled
./meminsight --config monitoring.conf \
             --upload-enable \
             --upload-interval 1800  # Upload every 30 minutes

# Configstore will contain:
# RUN_ITERATIONS=24
# RUN_INTERVAL=300
# UPLOAD_ENABLED=1
# UPLOAD_INTERVAL=1800
# OUTPUT_DIR=/var/log/meminsight
```

## 📊 Report Metadata

Every report file (CSV and JSON) begins with a metadata row containing the following fields:

| Field | Description |
|-------|-------------|
| `FIRMWARE_NAME` | Firmware/image name of the device |
| `MAC_ADDRESS` | MAC address of the primary network interface |
| `TIMESTAMP` | UTC timestamp at time of report generation |
| `UPTIME` | System uptime in seconds at report time |
| `KERNEL_VERSION` | Kernel version string (captured once at startup) |
| `REPORT_VERSION` | MemInsight report schema version |
| `ITERATION` | Current iteration number (1-based) within this run |
| `RUN_ITERATIONS` | Total iterations configured for this run |
| `RUN_INTERVAL` | Interval in seconds between iterations |
| `RUN_ID` | Per-run identifier built as `<epoch_seconds><pid><2-digit-random-suffix>` |

The `RUN_ID` groups all report files from the same invocation together, making it possible to correlate data across iterations without relying on timestamps alone.

## 📦 Integration Samples

- Sample systemd unit file: `deploy/systemd/meminsight.service` (main capture service)
- Sample Yocto recipe: `deploy/yocto/meminsight.bb` (includes all units)

Note: upload path/service unit samples are platform-integration artifacts and are not versioned in this repository.



## 🤖 Agent Customization Layout

The repository includes a project-scoped customization layout under `.github/`:

- Workspace instruction: `.github/copilot-instructions.md`
- File instructions: `.github/instructions/`
- Agent modes: `.github/agents/`
- Skills: `.github/skills/`

Each directory includes a local README for short usage guidance.

## 📐 OpenSpec Workflow

OpenSpec is the primary behavior source-of-truth for this repository.

- Documentation index: `docs/README.md`

- Directory overview: `openspec/README.md`
- Configuration reference: `openspec/config.yaml`
- Architecture baseline: `openspec/architecture/00-baseline-architecture.md`
- Capability specs usage: `openspec/specs/README.md`
- Change delta workflow: `openspec/changes/README.md`
- Detailed guide: `docs/OPENSPEC_USAGE_GUIDE.md`

Use OpenSpec as follows:

1. Read system-level context in `openspec/architecture/` for larger changes.
2. Read impacted capabilities in `openspec/specs/` before coding.
3. For behavior changes, create/update a delta under `openspec/changes/` first.
4. Keep implementation, tests, and capability docs in parity.

OpenSpec lifecycle shortcuts:

1. `/opsx:propose`
2. `/opsx:explore`
3. `/opsx:apply`
4. `/opsx:archive`

## 👥 Role-Based Workflow

Role-specific operating guidance is documented in:

- `docs/ROLE_BASED_WORKFLOW_GUIDE.md`

The guide defines workflows for:

1. Reviewer
2. Developer/Contributor
3. Architect Owner
4. Tester
5. Technical Documentation Expert

## 🧭 Agents and Skills

Agent and skill usage is documented in:

- `.github/AGENTS_AND_SKILLS_USAGE.md`

Primary agent modes:

- `.github/agents/meminsight-implementer.agent.md`
- `.github/agents/meminsight-reviewer.agent.md`

Primary skills:

- `.github/skills/openspec-propose/`
- `.github/skills/openspec-explore/`
- `.github/skills/openspec-apply-change/`
- `.github/skills/openspec-archive-change/`
- `.github/skills/openspec-source-of-truth/`
- `.github/skills/diagnose/`
- `.github/skills/tdd/`
- `.github/skills/to-issues-openspec/`
- `.github/skills/zoom-out/`
- `.github/skills/grill-with-docs-openspec/`
- `.github/skills/proc-fragmentation-compat/`
- `.github/skills/report-schema-compat/`

## 🧪 CI Workflows

Workflows are intentionally split by responsibility:

- `.github/workflows/native_full_build.yml`
   - Build-only workflow using `cov_build.sh --clean` then `cov_build.sh --enable-cjson --test`
- `.github/workflows/unit-test.yml`
   - Builds with the same flags and runs fixture tests through `test/run_ut.sh`

## 🔧 Troubleshooting

### Common Issues

**Issue**: Permission denied errors
```bash
# Solution: Run with appropriate privileges
sudo ./meminsight
# Or adjust /proc permissions
```

**Issue**: High memory usage during monitoring
```bash
# Solution: Reduce monitoring frequency or use whitelisting
./meminsight --interval 300 --config lightweight.conf
```

**Issue**: Upload service not triggering
```bash
# Verify systemd units are installed and active
sudo systemctl status meminsight-upload.path
sudo systemctl start meminsight-upload.path

# Check marker file creation
ls -la /tmp/.meminsight_upload

# Verify upload service runs after marker is created
sudo journalctl -u meminsight-upload.service -n 20
```

**Issue**: Configstore not found by upload script
```bash
# Check configstore file and permissions
ls -la /tmp/.meminsight_configstore
cat /tmp/.meminsight_configstore  # View current run parameters

# Ensure upload script has read permission
chmod 644 /tmp/.meminsight_configstore
```

**Issue**: In-progress sentinel not cleaned up
```bash
# Check for stalled runs
ls -la /tmp/.meminsight_inprogress

# If meminsight crashed, manually remove:
rm -f /tmp/.meminsight_inprogress

# Note: New run will recreate the sentinel automatically
```

## 🤝 Contributing

We welcome contributions! Please follow these guidelines:

1. **Fork the repository**
2. **Create a working branch**
3. **Follow coding standards**
   - Use consistent indentation (4 spaces)
   - Add comprehensive comments
   - Include error handling
4. **Test thoroughly**
   - Run `make test`
   - Validate on different systems
5. **Submit pull request**



---