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
- **Multiple Output Formats** - CSV export with timestamps and metadata
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
# Build the tool
./configure && make

# Run basic system-wide memory analysis
./meminsight

# Run a finite capture (overrides defaults)
./meminsight --iterations 10 --interval 30

# Include kernel threads in analysis
./meminsight --all

# Write reports to a custom directory
./meminsight --output /tmp/memreports --iterations 3
```

## Development Setup

```bash
# Setup development environment
autoreconf -fiv
./configure --enable-debug
make

# Run test fixtures (requires TESTME build)
make clean && make CFLAGS="-DTESTME"
./run_ut.sh
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
   -o, --output DIR            Output directory for generated CSV files (default: /tmp/meminsight)
         --iterations N          Number of iterations to run (overrides config)
         --interval SECONDS      Seconds between iterations (overrides config)
   -t, --test SMAPS MEMINFO     Run in test mode using supplied sample files (requires TESTME build)
   -h, --help                  Show help message and exit
```

### Default Behavior

If you run `./meminsight` with no flags, it runs in a long-running mode (indefinite iterations) with a 15-minute interval and writes CSV reports under `/tmp/meminsight/`.

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
./meminsight

# Custom interface via compile-time flag
CPPFLAGS="-DDEVICE_IDENTIFIER=\"erouter0\"" make clean && make
```

## 🔬 Advanced Features

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

# Run using sample fixtures
./meminsight --test tst/1-non-zero-swap-entry/meminsight_testSmap.txt tst/1-non-zero-swap-entry/meminsight_testMeminfo.txt

# Run the repository unit-test runner (executes all fixtures and a negative test)
./run_ut.sh
```

The `tst/` directory contains per-test subdirectories (e.g. `tst/1-non-zero-swap-entry/`) holding:
- `meminsight_testSmap.txt`
- `meminsight_testMeminfo.txt`

There is also a negative fixture in `tst/4-negative-duplicate-meminfo-field/` that is expected to fail in test mode and emit:
`Test Failed..meminfoHeader vs tstmeminfoHeader ...`

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
CPPFLAGS="-DDEVICE_IDENTIFIER=\"eth0\"" make clean && make
./meminsight --iterations 24 --interval 3600 --output /mnt/logs/
```

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