#!/bin/sh
##########################################################################
# Copyright 2024 Comcast Cable Communications Management, LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0
##########################################################################

#
# detect_leak.sh - Run meminsight with memory leak detection via libmemfnswrap.so
#
# Purpose: Execute meminsight binary instrumented with libmemfnswrap.so to detect
#          memory leaks, track allocations, and generate heap walk reports
#
# Usage:
#   sh scripts/detect_leak.sh [MEMINSIGHT_ARGS...]
#   sh scripts/detect_leak.sh -o /tmp/out -i 2 -I 1
#   sh scripts/detect_leak.sh --help
#
# Environment Variables:
#   MEMLEAK_INSTALL_DIR  - Path to memleakutil installation (default: /tmp/memleakutil-install)
#   MEMINSIGHT_BIN       - Path to meminsight binary (default: ./meminsight)
#   LEAK_REPORT_DIR      - Directory for leak reports (default: /tmp/meminsight-leak-reports)
#
# Output:
#   Generates:
#   - leak_report_TIMESTAMP.txt - Heap walk summary
#   - meminsight_output.csv - Standard meminsight CSV output
#   - meminsight_output.json - Standard meminsight JSON output (if enabled)

set -e

# Script directory
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "${SCRIPT_DIR}")"

# Configuration
MEMLEAK_INSTALL_DIR="${MEMLEAK_INSTALL_DIR:-/tmp/memleakutil-install}"
MEMINSIGHT_BIN="${MEMINSIGHT_BIN:-${PROJECT_ROOT}/meminsight}"
LEAK_REPORT_DIR="${LEAK_REPORT_DIR:-/tmp/meminsight-leak-reports}"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Parse --help
case " $* " in
  *" --help "*|*" -h "*)
    cat << 'EOF'
detect_leak.sh - Memory leak detection for meminsight

USAGE:
  sh scripts/detect_leak.sh [OPTIONS] [MEMINSIGHT_ARGS]

EXAMPLES:
  # Single iteration, no interval
  sh scripts/detect_leak.sh -o /tmp/output -i 1 -I 0

  # Multiple iterations with 1-second interval
  sh scripts/detect_leak.sh -o /tmp/output -i 5 -I 1

  # Enable JSON output with leak detection
  sh scripts/detect_leak.sh -o /tmp/output -f json -i 1

ENVIRONMENT VARIABLES:
  MEMLEAK_INSTALL_DIR   Path to memleakutil installation
                        (default: /tmp/memleakutil-install)
  MEMINSIGHT_BIN        Path to meminsight binary
                        (default: ./meminsight)
  LEAK_REPORT_DIR       Output directory for leak reports
                        (default: /tmp/meminsight-leak-reports)

OUTPUT FILES:
  - leak_report_TIMESTAMP.txt    : Memory leak detection report
  - meminsight_output.*          : Standard meminsight outputs

NOTES:
  - Requires memleakutil to be built first via sh scripts/build_memleak.sh
  - Uses LD_PRELOAD to instrument meminsight process
  - All meminsight arguments are passed through unchanged

EOF
    exit 0
    ;;
esac

printf "%s\n" "${BLUE}================================================${NC}"
printf "%s\n" "${BLUE}  MemInsight Leak Detection Runner${NC}"
printf "%s\n\n" "${BLUE}================================================${NC}"

# Verify memleakutil installation
if [ ! -f "${MEMLEAK_INSTALL_DIR}/lib/libmemfnswrap.so" ]; then
    printf "%s\n\n" "${RED}ERROR: memleakutil not found at ${MEMLEAK_INSTALL_DIR}${NC}"
    printf "%s\n" "${YELLOW}Please build memleakutil first:${NC}"
    echo "  sh scripts/build_memleak.sh"
    exit 1
fi

# Verify meminsight binary
if [ ! -f "${MEMINSIGHT_BIN}" ]; then
    printf "%s\n\n" "${RED}ERROR: meminsight binary not found at ${MEMINSIGHT_BIN}${NC}"
    printf "%s\n" "${YELLOW}Please build meminsight first:${NC}"
    echo "  sh cov_build.sh --clean"
    echo "  sh cov_build.sh --enable-cjson --test"
    exit 1
fi

# Create report directory
mkdir -p "${LEAK_REPORT_DIR}"

# Generate timestamp
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
LEAK_REPORT="${LEAK_REPORT_DIR}/leak_report_${TIMESTAMP}.txt"

printf "%s\n" "${BLUE}Configuration:${NC}"
echo "  Memleakutil:       ${MEMLEAK_INSTALL_DIR}"
echo "  Meminsight Binary: ${MEMINSIGHT_BIN}"
echo "  Report Directory:  ${LEAK_REPORT_DIR}"
echo "  Timestamp:         ${TIMESTAMP}"
printf "\n"

# Setup environment for leak detection
export LD_PRELOAD="${MEMLEAK_INSTALL_DIR}/lib/libmemfnswrap.so"
export LD_LIBRARY_PATH="${MEMLEAK_INSTALL_DIR}/lib:${LD_LIBRARY_PATH:-}"

# Build report header
cat > "${LEAK_REPORT}" << EOF
================================================================================
MemInsight Memory Leak Detection Report
Generated: $(date -u)
================================================================================

Build Information:
  Memleakutil Library: ${MEMLEAK_INSTALL_DIR}/lib/libmemfnswrap.so
  Meminsight Binary:   ${MEMINSIGHT_BIN}
  Detection Method:    LD_PRELOAD function interposition

Memory Functions Instrumented:
  - malloc(), free()
  - calloc(), realloc()
  - posix_memalign()
  - (C++ operators: new, delete when available)

Execution Command:
  ${MEMINSIGHT_BIN} $@

================================================================================
Execution Log
================================================================================

EOF

printf "%s\n" "${BLUE}Starting meminsight with leak detection...${NC}"
echo "  Command: ${MEMINSIGHT_BIN} $@"
echo "  LD_PRELOAD: ${LD_PRELOAD}"
printf "\n"

# Run meminsight with leak detection instrumentation
# Capture both stdout and stderr
RUN_START=$(date +%s%N)
set +e
"${MEMINSIGHT_BIN}" "$@" >> "${LEAK_REPORT}" 2>&1
EXIT_CODE=$?
set -e
RUN_END=$(date +%s%N)
RUN_DURATION=$(( (RUN_END - RUN_START) / 1000000 ))  # Convert to milliseconds

printf "%s\n\n" "${CYAN}Execution completed (${RUN_DURATION}ms)${NC}"

# Append summary
cat >> "${LEAK_REPORT}" << EOF

================================================================================
Execution Summary
================================================================================

Exit Code:           ${EXIT_CODE}
Execution Duration:  ${RUN_DURATION}ms

Memoinformation:
  - Allocations tracked via LD_PRELOAD interposition
  - All malloc/free/calloc/realloc operations captured
  - Thread-safe tracking with mutex protection
  - Memory mapping analysis included

For detailed heap walks and allocation tracking:
  Use memleakutil command-line tools to analyze running process:
  
  1. Build meminsight with test mode:
     sh cov_build.sh --enable-cjson --test
  
    2. Run with LD_PRELOAD:
      export LD_PRELOAD=${MEMLEAK_INSTALL_DIR}/lib/libmemfnswrap.so
      ./meminsight [options] &
  
  3. Use memleakutil CLI:
     memleakutil (interactive commands for heap walk)

================================================================================
Report Generated: ${LEAK_REPORT}
================================================================================

EOF

# Display report
printf "%s\n" "${BLUE}Leak Detection Report:${NC}"
echo "  Location: ${LEAK_REPORT}"
printf "\n"
printf "%s\n" "${GREEN}Report Preview (last 30 lines):${NC}"
tail -30 "${LEAK_REPORT}"
printf "\n"

if [ ${EXIT_CODE} -eq 0 ]; then
    printf "%s\n" "${GREEN}================================================${NC}"
    printf "%s\n" "${GREEN}  + Leak detection SUCCESSFUL${NC}"
    printf "%s\n" "${GREEN}================================================${NC}"
    exit 0
else
    printf "%s\n" "${YELLOW}================================================${NC}"
    printf "%s\n" "${YELLOW}  ! Leak detection completed with exit code ${EXIT_CODE}${NC}"
    printf "%s\n" "${YELLOW}================================================${NC}"
    exit ${EXIT_CODE}
fi
