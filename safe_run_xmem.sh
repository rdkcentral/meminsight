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

# Simple runner for Valgrind cases for xmeminsight
# Prereqs: valgrind, yes

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BIN="${ROOT_DIR}/xmeminsight"
CONF="${ROOT_DIR}/sample.conf"

VALGRIND_OPTS="-s --leak-check=full --show-leak-kinds=all --track-origins=yes"

# Try to locate binary if not present at expected location
if [ ! -f "${BIN}" ]; then
  echo "xmeminsight not found at ${BIN}, searching the repo..."
  FOUND=$(find "${ROOT_DIR}" -type f -name xmeminsight -print -quit 2>/dev/null || true)
  if [ -n "${FOUND}" ]; then
    BIN="${FOUND}"
    echo "Found xmeminsight at: ${BIN}"
  else
    echo "ERROR: xmeminsight binary not found after build."
    find "${ROOT_DIR}" -maxdepth 6 -type f -name 'xmeminsight*' -print || true
    exit 1
  fi
fi

echo "Using binary: ${BIN}"
ls -la "${BIN}" || true
file "${BIN}" || true

# Show dynamic linker / missing libs info
if command -v readelf >/dev/null 2>&1; then
  echo "ELF program headers (interpreter if any):"
  readelf -l "${BIN}" 2>/dev/null | awk '/INTERP/ {print;exit}' || true
fi
if command -v ldd >/dev/null 2>&1; then
  echo "ldd output (may fail for non-native binaries):"
  ldd "${BIN}" 2>&1 || true
fi

echo "==== Case 1: direct args ===="
echo "Running: valgrind ${VALGRIND_OPTS} ${BIN} --interval 2 --iterations 5"
valgrind ${VALGRIND_OPTS} "${BIN}" --interval 2 --iterations 5 || echo "Case 1 exited non-zero"

echo
echo "==== Case 2: config file + background yes ===="
cat > "${CONF}" <<'EOF'
# sample.conf for xmeminsight
process_whitelist=1,yes
output_dir=/tmp/xmeminsight_reports
iterations=5
interval=2
log_level = INFO

EOF
echo "==== Conf file:"
cat ${CONF}
# start yes in background
yes >/dev/null 2>&1 &
YES_PID=$!

trap 'kill "${YES_PID}" >/dev/null 2>&1 || true; echo "cleaned up yes (pid=${YES_PID})"' EXIT

echo "Started yes (pid=${YES_PID}), running xmeminsight under valgrind with config ${CONF}"
valgrind ${VALGRIND_OPTS} "${BIN}" -c "${CONF}" || echo "Case 2 exited non-zero"

echo "Sample conf used: ${CONF}"
echo "==== All done ===="
 
