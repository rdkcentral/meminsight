# 🔍 xmeminsight

**xmeminsight** is a powerful, lightweight Linux memory profiling and monitoring tool designed for system administrators, developers, and embedded system engineers. It provides comprehensive real-time memory usage analysis with detailed per-process statistics and system-wide memory insights.

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
- **Multiple Output Formats** - CSV (default) and JSON export with timestamps and metadata
- **JSON Output Support** - Optional cJSON-based structured data export with pretty-print capability
- **Kernel Thread Support** - Optional inclusion of kernel threads
- **Long-running Mode** - Extended monitoring with automatic intervals

### 🔧 **Advanced Capabilities**
- **Network Interface Detection** - Automatic MAC address retrieval
- **Firmware Information** - System version and device property extraction
- **Memory Leak Detection** - Track memory growth over time
- **Performance Profiling** - CPU and memory correlation analysis
- **Embedded System Optimized** - Minimal overhead for resource-constrained environments

## 🚀 Quick Start

```bash
# Build the tool (CSV-only, minimal build)
./cov_build.sh

# Build with JSON support
./cov_build.sh --enable-cjson

# Run basic system-wide memory analysis (CSV output)
./xmeminsight

# Run with JSON output
./xmeminsight --fmt json --iterations 10 --interval 30

# Run with pretty-printed JSON
./xmeminsight --fmt json --json-pretty --iterations 3

# Include kernel threads in analysis
./xmeminsight --all

# Write reports to a custom directory
./xmeminsight --output /tmp/memreports --iterations 3
```

## Development Setup

```bash
# Build with all features for development
./cov_build.sh --enable-cjson --enable-test

# Or manually with autotools
autoreconf -fiv
./configure --enable-cjson --enable-test
make

# Run test fixtures (requires --enable-test build)
./run_ut.sh

# Clean all build artifacts
./cov_build.sh --clean
```

## 🔨 Installation

### Prerequisites
- Linux operating system (kernel 2.6+)
- GCC compiler
- GNU Autotools (autoconf, automake)
- Standard C library with POSIX support
- **Optional:** libcjson-dev (for JSON output support)

### Building from Source

#### Using the Build Script (Recommended)

```bash
# Clone or extract the source code
cd meminsight/

# Basic build (CSV output only)
./cov_build.sh

# Build with JSON support (requires libcjson-dev)
./cov_build.sh --enable-cjson

# Build with test mode enabled
./cov_build.sh --enable-test

# Build with all features
./cov_build.sh --enable-cjson --enable-test

# Clean all build artifacts
./cov_build.sh --clean
```

#### Manual Build with Autotools

```bash
# Install cJSON library (for JSON support)
# Ubuntu/Debian:
sudo apt-get install libcjson-dev
# RHEL/CentOS:
sudo yum install cjson-devel

# Generate configure script
autoreconf -fiv

# Configure build environment
./configure --enable-cjson  # Add --enable-test for test mode

# Compile the binary
make

# Optional: Install system-wide
sudo make install
```

## 📖 Usage

### Command Line Options

```bash
xmeminsight [OPTIONS]

OPTIONS:
   -a, --all                         Include kernel threads for process monitoring
   -c, --config FILE                 Path to configuration file (must end with .conf)
   -o, --output DIR                  Output directory for generated files (default: /tmp/meminsight)
       --iterations N                Number of iterations to run (overrides config)
       --interval SECONDS            Seconds between iterations (overrides config)
       --fmt <format>                Specify report format: csv (default) or json
                                     (requires build with --enable-cjson)
       --json-pretty                 Pretty-print JSON output (only with --fmt json)
   -t, --test SMAPS MEMINFO          Run in test mode using supplied sample files
                                     (requires build with --enable-test)
   -h, --help                        Show help message and exit

NOTE: --fmt and --json-pretty options are only available when built with --enable-cjson
      --test option is only available when built with --enable-test
```

### Default Behavior

If you run `./xmeminsight` with no flags, it runs in a long-running mode (indefinite iterations) with a 15-minute interval and writes CSV reports under `/tmp/meminsight/`.

To run a finite capture, specify `--iterations` and/or `--interval`.

### Basic Usage Examples

```bash
# Single snapshot of system memory (CSV format)
./xmeminsight --iterations 1 --interval 0

# Single snapshot with JSON output (requires --enable-cjson build)
./xmeminsight --fmt json --iterations 1 --interval 0

# Pretty-printed JSON output
./xmeminsight --fmt json --json-pretty --iterations 5 --interval 60

# Monitor for 1 hour with 5-minute intervals
./xmeminsight --iterations 12 --interval 300

# Continuous monitoring with kernel threads
./xmeminsight --all

# JSON output to custom directory
./xmeminsight --fmt json --output /var/log/memory --iterations 10
```

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
| `output_dir` | Directory for output files | `/tmp/meminsight` | `/var/log/monitoring` |
| `iterations` | Number of sampling cycles | `1` | `10` |
| `interval` | Seconds between samples | `5` | `60` |
| `log_level` | Logging verbosity (parsed from config; current builds may not print logs unless debug is enabled) | `INFO` | `DEBUG`, `ERROR` |

### Network Interface Configuration

The tool automatically detects network interfaces for MAC address collection:

```bash
# Default: uses "eth0"
./xmeminsight

# Custom interface via compile-time flag
CPPFLAGS="-DDEVICE_IDENTIFIER=\"erouter0\"" make clean && make
```

## 🔬 Advanced Features

### Memory Leak Detection

```bash
# Monitor memory growth over 8 hours
./xmeminsight --iterations 48 --interval 600 --config leak_detection.conf
```

### Process-Specific Monitoring

```bash
# Monitor only critical services
echo "process_whitelist=systemd,NetworkManager,sshd" > services.conf
./xmeminsight --config services.conf --iterations 60 --interval 60
```

### Test Mode for Validation

```bash
# Build with test-mode enabled (required for -t/--test)
./cov_build.sh --enable-test

# Or with all features
./cov_build.sh --enable-cjson --enable-test

# Run using sample fixtures
./xmeminsight --test tst/1-non-zero-swap-entry/meminsight_testSmap.txt tst/1-non-zero-swap-entry/meminsight_testMeminfo.txt

# Run the repository unit-test runner (executes all fixtures and a negative test)
./run_ut.sh
```

The `tst/` directory contains per-test subdirectories (e.g. `tst/1-non-zero-swap-entry/`) holding:
- `meminsight_testSmap.txt`
- `meminsight_testMeminfo.txt`

There is also a negative fixture in `tst/4-negative-duplicate-meminfo-field/` that is expected to fail in test mode and emit:
`Test Failed..meminfoHeader vs tstmeminfoHeader ...`

## 🏗️ Build System

### Build Script (cov_build.sh)

The project provides a convenient build script that wraps autotools:

```bash
# Show help
./cov_build.sh --help

# Minimal build (CSV only)
./cov_build.sh

# Enable JSON output support
./cov_build.sh --enable-cjson

# Enable test mode
./cov_build.sh --enable-test

# Enable both features
./cov_build.sh --enable-cjson --enable-test

# Clean all artifacts
./cov_build.sh --clean
```

### Autotools Configuration

The project uses GNU Autotools for cross-platform compatibility:

- **`configure.ac`** - Autoconf configuration with optional features
- **`Makefile.am`** - Automake build rules with conditional linking
- **`config.h`** - Generated header with feature flags (ENABLE_CJSON, TESTME)
- **Generated files** - `configure`, `Makefile.in`

### Configure Options

```bash
# Enable cJSON support for JSON output
./configure --enable-cjson

# Enable test mode with TESTME flag
./configure --enable-test

# Combine multiple options
./configure --enable-cjson --enable-test
```

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

### Conditional Compilation

The build system uses conditional compilation for optional features:

- **ENABLE_CJSON** - Defined when `--enable-cjson` is used
  - Enables `--fmt json` and `--json-pretty` CLI options
  - Links against `-lcjson` library
  - Compiles JSON output functions

- **TESTME** - Defined when `--enable-test` is used
  - Enables `--test` CLI option for fixture-based testing
  - Includes test validation code
  - Used by `run_ut.sh` test runner

## 🏛️ Architecture

### Memory Collection Pipeline

1. **Process Discovery** - Scan `/proc` filesystem
2. **Statistics Parsing** - Read `/proc/[pid]/stat` and `/proc/[pid]/smaps`
3. **Data Aggregation** - Calculate totals and averages
4. **Sorting & Filtering** - Apply whitelist and sort by PSS
5. **Output Generation** - Write CSV with metadata

### Key Functions

| Function | Purpose | Location |
|----------|---------|----------|
| `collectSystemMemoryStats()` | Main collection orchestrator | memstatus.c |
| `getProcessInfos()` | Parse per-process smaps data | memstatus.c |
| `fillProcessStatFields()` | Extract stat file information | memstatus.c |
| `addProcessInfo()` | Maintain sorted process list | memstatus.c |
| `getMacAddress()` | Network interface detection | memstatus.c |
| `parseConfig()` | Configuration file processing | memstatus.c |

---

## 💡 Examples

### Example 1: Web Server Monitoring

```bash
# Monitor web server processes (CSV output)
cat > webserver.conf << EOF
process_whitelist=nginx,apache2,httpd,php-fpm
output_dir=/var/log/webserver-memory
iterations=288
interval=300
log_level=INFO
EOF

./xmeminsight --config webserver.conf

# Same with JSON output (requires --enable-cjson build)
./xmeminsight --fmt json --json-pretty --config webserver.conf
```

### Example 2: Database Performance Analysis

```bash
# Monitor database memory usage every minute for 2 hours (CSV)
./xmeminsight --iterations 120 --interval 60 \
              --output /tmp/db-analysis \
              --config database.conf

# Export as structured JSON for automated analysis
./xmeminsight --fmt json --iterations 120 --interval 60 \
              --output /tmp/db-analysis \
              --config database.conf
```

### Example 3: Embedded System Monitoring

```bash
# Minimal build for resource-constrained devices (CSV only, no test code)
./cov_build.sh

# If custom network interface is needed
autoreconf -fiv
CPPFLAGS="-DDEVICE_IDENTIFIER=\"erouter0\"" ./configure
make

# Lightweight monitoring
./xmeminsight --iterations 24 --interval 3600 --output /mnt/logs/
```

### Example 4: JSON Export for Data Pipelines

```bash
# Build with JSON support
./cov_build.sh --enable-cjson

# Generate machine-readable JSON reports for log aggregation
./xmeminsight --fmt json \
              --iterations 1440 \
              --interval 60 \
              --output /var/log/meminsight/

# Process JSON with jq or send to monitoring systems
cat /tmp/meminsight/*.json | jq '.processes[] | select(.pss > 100000)'
```

## 🔧 Troubleshooting

### Common Issues

**Issue**: Permission denied errors
```bash
# Solution: Run with appropriate privileges
sudo ./xmeminsight
# Or adjust /proc permissions
```

**Issue**: High memory usage during monitoring
```bash
# Solution: Reduce monitoring frequency or use whitelisting
./xmeminsight --interval 300 --config lightweight.conf
```

## 🤝 Contributing

We welcome contributions! Please follow these guidelines:

1. **Fork the repository**
2. **Create a feature branch**
3. **Follow coding standards**
   - Use consistent indentation (4 spaces)
   - Add comprehensive comments
   - Include error handling
4. **Test thoroughly**
   - Run `make test`
   - Validate on different systems
5. **Submit pull request**



---