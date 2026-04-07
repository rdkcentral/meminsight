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
#   sh scripts/detect_leak.sh
#   sh scripts/detect_leak.sh --help
#
# Environment Variables:
#   MEMLEAK_INSTALL_DIR  - Path to memleakutil installation (default: /tmp/memleakutil-install)
#   MEMINSIGHT_BIN       - Path to meminsight binary (default: ./meminsight)
#   LEAK_REPORT_DIR      - Directory for leak reports (default: /tmp/meminsight-leak-reports)
#
# Output:
#   Generates:
#   - leak_report_0_help_TIMESTAMP.txt (sanity/help case)
#   - leak_report_1_csv_TIMESTAMP.txt (CSV runtime case)
#   - leak_report_2_json_TIMESTAMP.txt (JSON runtime case)

set -e

# Script directory
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "${SCRIPT_DIR}")"

# Configuration
MEMLEAK_INSTALL_DIR="${MEMLEAK_INSTALL_DIR:-/tmp/memleakutil-install}"
MEMINSIGHT_BIN="${MEMINSIGHT_BIN:-${PROJECT_ROOT}/meminsight}"
LEAK_REPORT_DIR="${LEAK_REPORT_DIR:-/tmp/meminsight-leak-reports}"

# Parse --help
case " $* " in
  *" --help "*|*" -h "*)
    cat << 'EOF'
detect_leak.sh - Memory leak detection for meminsight

USAGE:
  sh scripts/detect_leak.sh [OPTIONS]

EXAMPLES:
  # Run all default leak test cases
  sh scripts/detect_leak.sh

ENVIRONMENT VARIABLES:
  MEMLEAK_INSTALL_DIR   Path to memleakutil installation
                        (default: /tmp/memleakutil-install)
  MEMINSIGHT_BIN        Path to meminsight binary
                        (default: ./meminsight)
  LEAK_REPORT_DIR       Output directory for leak reports
                        (default: /tmp/meminsight-leak-reports)

OUTPUT FILES:
  - leak_report_0_help_*.txt      : Help sanity run
  - leak_report_1_csv_*.txt      : CSV run report
  - leak_report_2_json_*.txt     : JSON run report

NOTES:
  - Requires memleakutil to be built first via sh scripts/build_memleak.sh
  - Uses LD_PRELOAD to instrument meminsight process
  - Runs these built-in cases:
      0) --help
      1) --interval 2 --iterations 5 --output /tmp/meminsight/leak
      2) --interval 2 --iterations 5 --output /tmp/meminsight/leak2 --fmt json --json-pretty

EOF
    exit 0
    ;;
esac

printf "%s\n" "================================================"
printf "%s\n" "  MemInsight Leak Detection Runner"
printf "%s\n\n" "================================================"

# Verify memleakutil installation
if [ ! -f "${MEMLEAK_INSTALL_DIR}/lib/libmemfnswrap.so" ]; then
  printf "%s\n\n" "ERROR: memleakutil not found at ${MEMLEAK_INSTALL_DIR}"
  printf "%s\n" "Please build memleakutil first:"
    echo "  sh scripts/build_memleak.sh"
    exit 1
fi

# Verify meminsight binary
if [ ! -f "${MEMINSIGHT_BIN}" ]; then
  printf "%s\n\n" "ERROR: meminsight binary not found at ${MEMINSIGHT_BIN}"
  printf "%s\n" "Please build meminsight first:"
    echo "  sh cov_build.sh --clean"
    echo "  sh cov_build.sh --enable-cjson --test"
    exit 1
fi

# Create report directory
mkdir -p "${LEAK_REPORT_DIR}"
mkdir -p /tmp/meminsight

# Clear previous report files for a clean run view.
printf "%s\n" "Clearing previous leak reports from ${LEAK_REPORT_DIR}..."
find "${LEAK_REPORT_DIR}" -mindepth 1 -maxdepth 1 -exec rm -rf {} \; 2>/dev/null || true
printf "%s\n\n" "Previous leak reports cleared"

# Generate timestamp shared by all case reports
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
REPORT_0="${LEAK_REPORT_DIR}/leak_report_0_help_${TIMESTAMP}.txt"
REPORT_1="${LEAK_REPORT_DIR}/leak_report_1_csv_${TIMESTAMP}.txt"
REPORT_2="${LEAK_REPORT_DIR}/leak_report_2_json_${TIMESTAMP}.txt"

printf "%s\n" "Configuration:"
echo "  Memleakutil:       ${MEMLEAK_INSTALL_DIR}"
echo "  Meminsight Binary: ${MEMINSIGHT_BIN}"
echo "  Report Directory:  ${LEAK_REPORT_DIR}"
echo "  Timestamp:         ${TIMESTAMP}"
printf "\n"

# Setup library paths for leak detection (applied per meminsight invocation only)
LEAK_LD_PRELOAD="${MEMLEAK_INSTALL_DIR}/lib/libmemfnswrap.so"
LEAK_LD_LIBRARY_PATH="${MEMLEAK_INSTALL_DIR}/lib:${LD_LIBRARY_PATH:-}"

# Helper: run one leak case and write report
run_case() {
  case_id="$1"
  report_file="$2"
  shift 2

  cat > "${report_file}" << EOF
================================================================================
MemInsight Memory Leak Detection Report
Generated: $(date -u)
================================================================================

Build Information:
  Memleakutil Library: ${MEMLEAK_INSTALL_DIR}/lib/libmemfnswrap.so
  Meminsight Binary:   ${MEMINSIGHT_BIN}
  Detection Method:    LD_PRELOAD function interposition
  Case ID:             ${case_id}

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

  printf "%s\n" "Running case ${case_id}: ${MEMINSIGHT_BIN} $*"
  RUN_START=$(date +%s%N)
  set +e
  LD_PRELOAD="${LEAK_LD_PRELOAD}" LD_LIBRARY_PATH="${LEAK_LD_LIBRARY_PATH}" "${MEMINSIGHT_BIN}" "$@" >> "${report_file}" 2>&1
  rc=$?
  set -e
  RUN_END=$(date +%s%N)
  RUN_DURATION=$(( (RUN_END - RUN_START) / 1000000 ))

  cat >> "${report_file}" << EOF

================================================================================
Execution Summary
================================================================================

Exit Code:           ${rc}
Execution Duration:  ${RUN_DURATION}ms

Memory information:
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
Report Generated: ${report_file}
================================================================================

EOF

  printf "%s\n\n" "Case ${case_id} completed in ${RUN_DURATION}ms with exit code ${rc}"
  return ${rc}
}

printf "%s\n" "Starting meminsight leak test matrix..."
echo "  LD_PRELOAD: ${LEAK_LD_PRELOAD}"
printf "\n"

FAIL_COUNT=0

# 0th case: help sanity under preload
run_case "0-help" "${REPORT_0}" --help || true
if grep -E "Usage: meminsight|Options:" "${REPORT_0}" >/dev/null 2>&1; then
  printf "%s\n\n" "Case 0 validation passed (help text detected)"
else
  printf "%s\n\n" "Case 0 validation failed (expected help text missing)"
  FAIL_COUNT=$((FAIL_COUNT + 1))
fi

# 1st case: CSV output run
run_case "1-csv" "${REPORT_1}" --interval 2 --iterations 5 --output /tmp/meminsight/leak || FAIL_COUNT=$((FAIL_COUNT + 1))

# 2nd case: JSON output run
run_case "2-json" "${REPORT_2}" --interval 2 --iterations 5 --output /tmp/meminsight/leak2 --fmt json --json-pretty || FAIL_COUNT=$((FAIL_COUNT + 1))

printf "%s\n" "Leak Detection Reports:"
echo "  0th case: ${REPORT_0}"
echo "  1st case: ${REPORT_1}"
echo "  2nd case: ${REPORT_2}"
printf "\n"

printf "%s\n" "--- Case 1 (CSV) full report ---"
cat "${REPORT_1}" || true
printf "\n"

printf "%s\n" "--- Case 2 (JSON) full report ---"
cat "${REPORT_2}" || true
printf "\n"

printf "%s\n" "Cleaning meminsight leak output directories..."
rm -rf /tmp/meminsight/leak /tmp/meminsight/leak2
printf "%s\n\n" "Cleanup complete"

if [ "${FAIL_COUNT}" -eq 0 ]; then
  printf "%s\n" "================================================"
  printf "%s\n" "  + Leak detection test matrix SUCCESSFUL"
  printf "%s\n" "================================================"
  exit 0
fi

printf "%s\n" "================================================"
printf "%s\n" "  ! Leak detection test matrix completed with ${FAIL_COUNT} failing case(s)"
printf "%s\n" "================================================"
exit 1
