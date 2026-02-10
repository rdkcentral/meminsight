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

echo "=========================================="
echo "Running xMemInsight Unit Tests"
echo "=========================================="

# Find xmeminsight binary location
XMEM_BIN=""
if command -v xmeminsight >/dev/null 2>&1; then
    XMEM_BIN=$(which xmeminsight)
elif [ -f "./xmeminsight" ]; then
    XMEM_BIN="./xmeminsight"
fi

if [ ! -f "$XMEM_BIN" ]; then
    echo "ERROR: xmeminsight binary not found!"
    echo "Searched in PATH and current directory"
    exit 1
fi

echo "Using xmeminsight binary: $XMEM_BIN"

# Clean up any previous test outputs
rm -rf /tmp/meminsight
mkdir -p /tmp/meminsight

TEST_FAILED=0

# “Array-like” lists (POSIX sh-friendly): index with cut -d'|' -fN
TEST_DESCRIPTIONS="Non-zero Swap and SwapPSS|Swap and SwapPSS with 0 value|Empty Swap and SwapPSS values"
TEST_DIRS="1-non-zero-swap-entry|2-zero-value-swap-entry|3-empty-swap-entry"

NUM_TESTS=$(printf '%s' "$TEST_DIRS" | awk -F'|' '{print NF}')

i=1
while [ "$i" -le "$NUM_TESTS" ]; do
    DESC=$(printf '%s' "$TEST_DESCRIPTIONS" | cut -d'|' -f"$i")
    DIR=$(printf '%s' "$TEST_DIRS" | cut -d'|' -f"$i")
    SMAP_FILE="tst/$DIR/meminsight_testSmap.txt"
    MEMINFO_FILE="tst/$DIR/meminsight_testMeminfo.txt"

    echo "------------------------------------------"
    echo "Test $i: $DESC"
    echo "------------------------------------------"
    echo "Command: $XMEM_BIN -t $SMAP_FILE $MEMINFO_FILE"

    if $XMEM_BIN -t "$SMAP_FILE" "$MEMINFO_FILE"; then
        echo "✓ Test $i PASSED"
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
            TEST_FAILED=$((TEST_FAILED + 1))
        fi
    else
        echo "✗ Test $i FAILED"
        TEST_FAILED=$((TEST_FAILED + 1))
    fi
    echo ""
    # Clean up for next test
    rm -rf /tmp/meminsight/*.csv

    i=$((i + 1))
done

# Negative test: intentionally malformed meminfo fixture (duplicate needed field)
NEG_DESC="Negative: duplicate meminfo field triggers failure"
NEG_SMAP_FILE="tst/1-non-zero-swap-entry/meminsight_testSmap.txt"
NEG_MEMINFO_FILE="tst/4-negative-duplicate-meminfo-field/meminsight_testMeminfo.txt"
NEG_LOG_FILE="/tmp/meminsight/negative_test.log"

echo "------------------------------------------"
echo "$NEG_DESC"
echo "------------------------------------------"
echo "Command: $XMEM_BIN -t $NEG_SMAP_FILE $NEG_MEMINFO_FILE"

rm -rf /tmp/meminsight/*.csv

$XMEM_BIN -t "$NEG_SMAP_FILE" "$NEG_MEMINFO_FILE" >"$NEG_LOG_FILE" 2>&1
RC=$?

if [ "$RC" -eq 0 ]; then
    echo "✗ Negative test FAILED (unexpected success)"
    TEST_FAILED=$((TEST_FAILED + 1))
else
    if grep -F "Test Failed..meminfoHeader vs tstmeminfoHeader" "$NEG_LOG_FILE" >/dev/null 2>&1; then
        echo "✓ Negative test PASSED (expected failure observed)"
        # cat the log file for visibility
        echo "Log output:"
        cat "$NEG_LOG_FILE"
    else
        echo "✗ Negative test FAILED (missing expected log line)"
        echo "Log output:"
        cat "$NEG_LOG_FILE"
        TEST_FAILED=$((TEST_FAILED + 1))
    fi
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
echo "=========================================="
