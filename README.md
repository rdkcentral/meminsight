# MemInsight

MemInsight is a lightweight, configurable Linux tool for collecting detailed system and per-process memory and CPU statistics, tailored for embedded environments. It is designed to aid in the diagnosis and debugging of memory and performance issues.

## Features

- **System-wide and per-process memory statistics:**  
  Scans `/proc` for all processes and parses `/proc/[pid]/stat` and `/proc/[pid]/smaps` to collect:
  - Resident Set Size (RSS)
  - Proportional Set Size (PSS)
  - Shared clean memory
  - Private dirty memory
  - Swap PSS
  - Major page faults
  - CPU time (user + system)

- **Configurable output:**  
  Supports configuration files to specify process whitelists, output file location, number of iterations, interval between samples, and log level.

- **CSV output:**  
  Outputs results in CSV format for easy analysis.

- **Test mode:**  
  Includes a test mode for validating parsing logic and linked list sorting.

- **Kernel thread support:**  
  Optionally includes kernel threads in monitoring.

## Usage

### Building

To build the binary:

```sh
make
```

This produces the `xMemInsight` executable.

### Running

#### Standalone (system-wide monitoring):

```sh
make run
# or
./xMemInsight
```

#### Test mode:

```sh
make test
# or
./xMemInsight --test
```

#### Help:

```sh
make help
# or
./xMemInsight --help
```

#### With configuration file:

```sh
./xMemInsight --config /path/to/config.conf
```

### Configuration File Format

Supported extensions: `.conf`, `.cfg`, `.txt`

Example config file:

```
process_whitelist=myapp,systemd,1234,
output_file=/tmp/xmeminsight.csv,
iterations=10,
interval=60
log_level=INFO
```

- `process_whitelist`: Comma-separated list of process names or PIDs to monitor.
- `output_file`: Path to output CSV file.
- `iterations`: Number of times to sample statistics.
- `interval`: Seconds to wait between samples.
- `log_level`: Logging verbosity.

## Output

- **CSV file** containing per-process statistics and system-wide memory info.
- **Console output** for progress and errors.

## Code Structure

- `memstatus.c`: Main source file. Handles CLI parsing, process scanning, statistics collection, output writing, and configuration parsing.
- `memstatus.h`: Header file. Contains data structures, macros, global variables, and function prototypes.
- `Makefile`: Build and run targets.
- `README.md`: Documentation.

## Main Functions

- `main()`: Entry point. Parses arguments, loads config, runs monitoring.
- `systemWide()`: Collects system-wide stats.
- `parseConfig()`: Loads configuration from file.
- `getProcessInfos()`: Parses `/proc/[pid]/smaps` for memory stats.
- `fillProcessStatFields()`: Parses `/proc/[pid]/stat` for process info.
- `writeProcessInfo()`: Outputs and frees process info linked list.
- `addProcessInfo()`: Inserts process info into sorted linked list.
- `saveMeminfo()`: Dumps `/proc/meminfo` to output.

## Extending

- Add new fields to `Process_Info` struct for more statistics.
- Modify `parseConfig()` to support additional config options.
- Implement more advanced filtering or output formats as needed.

## License

This project is licensed under the Apache-2.0 License.

---