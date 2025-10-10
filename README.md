# 🔍 xmeminsight

**xmeminsight** is a powerful, lightweight Linux memory profiling and monitoring tool designed for system administrators, developers, and embedded system engineers. It provides comprehensive real-time memory usage analysis with detailed per-process statistics and system-wide memory insights.

---

## 📋 Table of Contents

- [Features](#-features)
- [Quick Start](#-quick-start)
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
- **Multiple Output Formats** - CSV and JSON export with timestamps and metadata (JSON requires cJSON)
- **Kernel Thread Support** - Optional inclusion of kernel threads
- **Long-running Mode** - Extended monitoring with automatic intervals

### 🔧 **Advanced Capabilities**
- **Network Interface Detection** - Automatic MAC address retrieval
- **Firmware Information** - System version and device property extraction
- **Optimized Memory Parsing** - Learning-based algorithm reduces parsing overhead
- **Embedded System Optimized** - Minimal overhead for resource-constrained environments

## 🚀 Quick Start

```bash
# Build the tool
./configure && make

# Run basic system-wide memory analysis
./xmeminsight

# Monitor specific processes with custom intervals
./xmeminsight --config myconfig.conf --iterations 10 --interval 30

# Include kernel threads in analysis
./xmeminsight --all --output /tmp/memreports

# Output in JSON format (requires cJSON support)
./xmeminsight --fmt json
```

## 🔨 Installation

### Prerequisites
- Linux operating system (kernel 2.6+)
- GCC compiler
- GNU Autotools (autoconf, automake)
- Standard C library with POSIX support
- libcjson-dev (optional, for JSON output support)

### Building from Source

```bash
# Clone or extract the source code
cd meminsight/

# Generate configure script
autoreconf -fiv

# Configure build environment (add --enable-cjson for JSON support)
./configure

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
  -v, --version              Display version information
  -a, --all                  Include kernel threads in analysis
  -c, --config FILE          Use configuration file
  --iterations N             Number of sampling iterations (default: 1)
  --interval SECONDS         Seconds between samples (default: 0)
  -o, --output DIR           Output directory (default: /tmp/meminsight)
  --fmt FORMAT               Output format: csv (default) or json
  -n, --numprocs COUNT       Limit output to top N processes by PSS (default: 15)
  -h, --help                 Show help message
  -t, --test                 Run internal test suite
```

### Basic Usage Examples

```bash
# Single snapshot of system memory
./xmeminsight

# Monitor for 1 hour with 5-minute intervals
./xmeminsight --iterations 12 --interval 300

# Continuous monitoring with configuration file
./xmeminsight --config production.conf

# Debug mode with kernel thread inclusion and JSON output
./xmeminsight --all --fmt json
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
| `interval` | Seconds between samples | `0` | `60` |
| `log_level` | Logging verbosity | `INFO` | `DEBUG`, `ERROR` |

### Network Interface Configuration

The tool automatically detects network interfaces for MAC address collection:

```bash
# Default: uses "eth0"
./xmeminsight

# Custom interface via compile-time flag
CPPFLAGS="-DDEVICE_IDENTIFIER=\"erouter0\"" make clean && make
```

## 🔬 Advanced Features

### JSON Output Format

When compiled with cJSON support, xmeminsight can output data in JSON format:

```bash
# Enable JSON output
./xmeminsight --fmt json
```

### Process Limiting

By default, xmeminsight shows only the top 15 processes by PSS usage:

```bash
# Show top 50 processes instead
./xmeminsight --numprocs 50

# Show all processes
./xmeminsight --numprocs 0
```

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

### Long-Running System Monitoring

```bash
# 24/7 monitoring with 15-minute intervals
./xmeminsight
```

### Test Mode for Validation

```bash
# Run internal test suite
./xmeminsight --test

# Validate configuration file
./xmeminsight --config test.conf --test
```

## 🏗️ Build System

### Autotools Configuration

The project uses GNU Autotools for cross-platform compatibility:

- **configure.ac** - Autoconf configuration
- **Makefile.am** - Automake build rules
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
5. **Output Generation** - Write CSV or JSON with metadata

### Optimized Memory Information Collection

xmeminsight uses a learning-based algorithm to efficiently parse system memory information:

1. **First Run**: Learns the structure and position of target fields in memory files
2. **Subsequent Runs**: Uses learned positions to directly access relevant data
3. **Early Termination**: Stops parsing once all target fields are found
4. **Minimal Comparisons**: Reduces string comparison overhead

### Key Functions

| Function | Purpose | Location |
|----------|---------|----------|
| `collectSystemMemoryStats()` | Main collection orchestrator | src/memstatus.c |
| `getProcessInfos()` | Parse per-process smaps data | src/memstatus.c |
| `fillProcessStatFields()` | Extract stat file information | src/memstatus.c |
| `addProcessInfo()` | Maintain sorted process list | src/memstatus.c |
| `getMacAddress()` | Network interface detection | src/memstatus.c |
| `parseConfig()` | Configuration file processing | src/memstatus.c |
| `saveMeminfo()` | Efficiently extract memory information | src/memstatus.c |
| `addMemInfoJSON()` | Generate JSON memory information | src/memstatus.c |
| `initializeSetup()` | Prepare MAC, firmware, and output paths | src/memstatus.c |

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

./xmeminsight --config webserver.conf
```

### Example 2: Database Performance Analysis with JSON Output

```bash
# Monitor database memory usage every minute for 2 hours
./xmeminsight --iterations 120 --interval 60 \
              --output /tmp/db-analysis \
              --config database.conf \
              --fmt json
```

### Example 3: Embedded System Monitoring

```bash
# Lightweight monitoring for embedded systems
CPPFLAGS="-DDEVICE_IDENTIFIER=\"eth0\"" make clean && make
./xmeminsight --iterations 24 --interval 3600 --output /mnt/logs/ --numprocs 10
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

**Issue**: Missing JSON support
```bash
# Solution: Rebuild with CJSON support
./configure --enable-cjson
make clean && make
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

### Development Setup

```bash
# Setup development environment
autoreconf -fiv
./configure --enable-debug
make

# Run test suite
./xmeminsight --test
```