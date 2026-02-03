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

set -e

echo "=========================================="
echo "Running xMemInsight Unit Tests"
echo "=========================================="

# Find xmeminsight binary location
XMEM_BIN=$(which xmeminsight 2>/dev/null || echo "./xmeminsight")

if [ ! -f "$XMEM_BIN" ]; then
    echo "ERROR: xmeminsight binary not found!"
    echo "Searched in PATH and current directory"
    exit 1
fi

echo "Using xmeminsight binary: $XMEM_BIN"
echo ""

# Clean up any previous test outputs
rm -rf /tmp/meminsight
mkdir -p /tmp/meminsight

TEST_FAILED=0

# Test 1: Non-zero Swap and SwapPSS
echo "=========================================="
echo "Test 1: Non-zero Swap and SwapPSS"
echo "=========================================="
echo "Command: $XMEM_BIN -t tst/meminsight_testSmap.txt tst/meminsight_testMeminfo.txt"
if $XMEM_BIN -t tst/meminsight_testSmap.txt tst/meminsight_testMeminfo.txt; then
    echo "✓ Test 1 PASSED"
    echo ""
    echo "Output file contents:"
    echo "---"
    CSV_FILE=$(ls /tmp/meminsight/*.csv 2>/dev/null | head -n 1)
    if [ -n "$CSV_FILE" ] && [ -f "$CSV_FILE" ]; then
        echo "File: $CSV_FILE"
        cat "$CSV_FILE"
        echo "---"
    else
        echo "WARNING: Output CSV file not found in /tmp/meminsight/"
        TEST_FAILED=1
    fi
else
    echo "✗ Test 1 FAILED"
    TEST_FAILED=1
fi
echo ""

# Clean up for next test
rm -rf /tmp/meminsight/*.csv

# Test 2: Swap and SwapPSS entries with 0 value
echo "=========================================="
echo "Test 2: Swap and SwapPSS with 0 value"
echo "=========================================="
echo "Command: $XMEM_BIN -t tst/meminsight_testSmap2.txt tst/meminsight_testMeminfo2.txt"
if $XMEM_BIN -t tst/meminsight_testSmap2.txt tst/meminsight_testMeminfo2.txt; then
    echo "✓ Test 2 PASSED"
    echo ""
    echo "Output file contents:"
    echo "---"
    CSV_FILE=$(ls /tmp/meminsight/*.csv 2>/dev/null | head -n 1)
    if [ -n "$CSV_FILE" ] && [ -f "$CSV_FILE" ]; then
        echo "File: $CSV_FILE"
        cat "$CSV_FILE"
        echo "---"
    else
        echo "WARNING: Output CSV file not found in /tmp/meminsight/"
        TEST_FAILED=1
    fi
else
    echo "✗ Test 2 FAILED"
    TEST_FAILED=1
fi
echo ""

# Clean up for next test
rm -rf /tmp/meminsight/*.csv

# Test 3: Empty Swap and SwapPSS values
echo "=========================================="
echo "Test 3: Empty Swap and SwapPSS values"
echo "=========================================="
echo "Command: $XMEM_BIN -t tst/meminsight_testSmap3.txt tst/meminsight_testMeminfo3.txt"
if $XMEM_BIN -t tst/meminsight_testSmap3.txt tst/meminsight_testMeminfo3.txt; then
    echo "✓ Test 3 PASSED"
    echo ""
    echo "Output file contents:"
    echo "---"
    CSV_FILE=$(ls /tmp/meminsight/*.csv 2>/dev/null | head -n 1)
    if [ -n "$CSV_FILE" ] && [ -f "$CSV_FILE" ]; then
        echo "File: $CSV_FILE"
        cat "$CSV_FILE"
        echo "---"
    else
        echo "WARNING: Output CSV file not found in /tmp/meminsight/"
        TEST_FAILED=1
    fi
else
    echo "✗ Test 3 FAILED"
    TEST_FAILED=1
fi
echo ""

# Summary
echo "=========================================="
echo "Test Summary"
echo "=========================================="
if [ $TEST_FAILED -eq 0 ]; then
    echo "✓ ALL TESTS PASSED"
    exit 0
else
    echo "✗ SOME TESTS FAILED"
    exit 1
fi
