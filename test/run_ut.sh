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
echo "Running meminsight Unit Tests"
echo "=========================================="

# Find meminsight binary location
MEM_BIN=""
if command -v meminsight >/dev/null 2>&1; then
    MEM_BIN=$(command -v meminsight)
elif [ -f "./meminsight" ]; then
    MEM_BIN="./meminsight"
fi

if [ ! -f "$MEM_BIN" ]; then
    echo "ERROR: meminsight binary not found!"
    echo "Searched in PATH and current directory"
    exit 1
fi

echo "Using meminsight binary: $MEM_BIN"

# Clean up any previous test outputs
rm -rf /tmp/meminsight
mkdir -p /tmp/meminsight

TEST_FAILED=0

# “Array-like” lists (POSIX sh-friendly): index with cut -d'|' -fN
TEST_DESCRIPTIONS="Non-zero Swap and SwapPSS|Swap and SwapPSS with 0 value|No Swap and SwapPSS values"
TEST_DIRS="1-non-zero-swap-entry|2-zero-value-swap-entry|3-empty-swap-entry"

NUM_TESTS=$(printf '%s' "$TEST_DIRS" | awk -F'|' '{print NF}')

i=1
while [ "$i" -le "$NUM_TESTS" ]; do
    DESC=$(printf '%s' "$TEST_DESCRIPTIONS" | cut -d'|' -f"$i")
    DIR=$(printf '%s' "$TEST_DIRS" | cut -d'|' -f"$i")
    SMAP_FILE="test/$DIR/meminsight_testSmap.txt"
    MEMINFO_FILE="test/$DIR/meminsight_testMeminfo.txt"

    echo "------------------------------------------"
    echo "Test $i: $DESC"
    echo "------------------------------------------"
    echo "Command: $MEM_BIN -o /tmp/meminsight -t $SMAP_FILE $MEMINFO_FILE"

    if $MEM_BIN -o /tmp/meminsight -t "$SMAP_FILE" "$MEMINFO_FILE"; then
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

# Fragmentation parsing test 1: pagetypeinfo preferred when both files are provided
FRAG_DESC1="Test 6: Fragmentation pagetypeinfo preferred"
FRAG_SMAP_FILE="test/1-non-zero-swap-entry/meminsight_testSmap.txt"
FRAG_MEMINFO_FILE="test/1-non-zero-swap-entry/meminsight_testMeminfo.txt"
FRAG_BUDDY_FILE="test/6-buddyinfo-sample/meminsight_testBuddyinfo.txt"
FRAG_PGT_FILE="test/7-pagetypeinfo-sample/meminsight_testPagetypeinfo.txt"

echo "------------------------------------------"
echo "$FRAG_DESC1"
echo "------------------------------------------"
echo "Command: $MEM_BIN --frag -o /tmp/meminsight -t $FRAG_SMAP_FILE $FRAG_MEMINFO_FILE $FRAG_BUDDY_FILE $FRAG_PGT_FILE"

rm -rf /tmp/meminsight/*.csv

if $MEM_BIN --frag -o /tmp/meminsight -t "$FRAG_SMAP_FILE" "$FRAG_MEMINFO_FILE" "$FRAG_BUDDY_FILE" "$FRAG_PGT_FILE"; then
    CSV_FILE=$(ls /tmp/meminsight/*.csv 2>/dev/null | head -n 1)
    if [ -n "$CSV_FILE" ] && grep -F "Fragmentation_PagetypeInfo:" "$CSV_FILE" >/dev/null 2>&1; then
        echo "✓ $FRAG_DESC1 PASSED"
    else
        echo "✗ $FRAG_DESC1 FAILED (pagetypeinfo section missing)"
        [ -n "$CSV_FILE" ] && cat "$CSV_FILE"
        TEST_FAILED=$((TEST_FAILED + 1))
    fi
else
    echo "✗ $FRAG_DESC1 FAILED (command execution failed)"
    TEST_FAILED=$((TEST_FAILED + 1))
fi
echo ""

# Fragmentation parsing test 2: buddyinfo fallback when pagetypeinfo fixture is omitted
FRAG_DESC2="Test 7: Fragmentation buddyinfo fallback"

echo "------------------------------------------"
echo "$FRAG_DESC2"
echo "------------------------------------------"
echo "Command: $MEM_BIN --frag -o /tmp/meminsight -t $FRAG_SMAP_FILE $FRAG_MEMINFO_FILE $FRAG_BUDDY_FILE"

rm -rf /tmp/meminsight/*.csv

if $MEM_BIN --frag -o /tmp/meminsight -t "$FRAG_SMAP_FILE" "$FRAG_MEMINFO_FILE" "$FRAG_BUDDY_FILE"; then
    CSV_FILE=$(ls /tmp/meminsight/*.csv 2>/dev/null | head -n 1)
    if [ -n "$CSV_FILE" ] && grep -F "Fragmentation_BuddyInfo:" "$CSV_FILE" >/dev/null 2>&1; then
        echo "✓ $FRAG_DESC2 PASSED"
    else
        echo "✗ $FRAG_DESC2 FAILED (buddyinfo section missing)"
        [ -n "$CSV_FILE" ] && cat "$CSV_FILE"
        TEST_FAILED=$((TEST_FAILED + 1))
    fi
else
    echo "✗ $FRAG_DESC2 FAILED (command execution failed)"
    TEST_FAILED=$((TEST_FAILED + 1))
fi
echo ""

# Fragmentation parsing test 3: buddyinfo kernel-format variant
FRAG_DESC3="Test 10: Fragmentation buddyinfo variant format"
FRAG_BUDDY_VARIANT_FILE="test/8-buddyinfo-variant-kernel/meminsight_testBuddyinfo.txt"

echo "------------------------------------------"
echo "$FRAG_DESC3"
echo "------------------------------------------"
echo "Command: $MEM_BIN --frag -o /tmp/meminsight -t $FRAG_SMAP_FILE $FRAG_MEMINFO_FILE $FRAG_BUDDY_VARIANT_FILE"

rm -rf /tmp/meminsight/*.csv

if $MEM_BIN --frag -o /tmp/meminsight -t "$FRAG_SMAP_FILE" "$FRAG_MEMINFO_FILE" "$FRAG_BUDDY_VARIANT_FILE"; then
    CSV_FILE=$(ls /tmp/meminsight/*.csv 2>/dev/null | head -n 1)
    if [ -n "$CSV_FILE" ] && grep -F "Fragmentation_BuddyInfo:" "$CSV_FILE" >/dev/null 2>&1; then
        echo "✓ $FRAG_DESC3 PASSED"
    else
        echo "✗ $FRAG_DESC3 FAILED (buddyinfo section missing)"
        [ -n "$CSV_FILE" ] && cat "$CSV_FILE"
        TEST_FAILED=$((TEST_FAILED + 1))
    fi
else
    echo "✗ $FRAG_DESC3 FAILED (command execution failed)"
    TEST_FAILED=$((TEST_FAILED + 1))
fi
echo ""

# Fragmentation parsing test 4: pagetypeinfo migration-layout variant
FRAG_DESC4="Test 11: Fragmentation pagetypeinfo variant format"
FRAG_PGT_VARIANT_FILE="test/9-pagetypeinfo-variant-layout/meminsight_testPagetypeinfo.txt"

echo "------------------------------------------"
echo "$FRAG_DESC4"
echo "------------------------------------------"
echo "Command: $MEM_BIN --frag -o /tmp/meminsight -t $FRAG_SMAP_FILE $FRAG_MEMINFO_FILE $FRAG_BUDDY_FILE $FRAG_PGT_VARIANT_FILE"

rm -rf /tmp/meminsight/*.csv

if $MEM_BIN --frag -o /tmp/meminsight -t "$FRAG_SMAP_FILE" "$FRAG_MEMINFO_FILE" "$FRAG_BUDDY_FILE" "$FRAG_PGT_VARIANT_FILE"; then
    CSV_FILE=$(ls /tmp/meminsight/*.csv 2>/dev/null | head -n 1)
    if [ -n "$CSV_FILE" ] && grep -F "Fragmentation_PagetypeInfo:" "$CSV_FILE" >/dev/null 2>&1; then
        echo "✓ $FRAG_DESC4 PASSED"
    else
        echo "✗ $FRAG_DESC4 FAILED (pagetypeinfo section missing)"
        [ -n "$CSV_FILE" ] && cat "$CSV_FILE"
        TEST_FAILED=$((TEST_FAILED + 1))
    fi
else
    echo "✗ $FRAG_DESC4 FAILED (command execution failed)"
    TEST_FAILED=$((TEST_FAILED + 1))
fi
echo ""

# Fault-injection test: both optional fragmentation sources missing in TESTME
FRAG_DESC5="Test 12 (Fault Injection): Missing buddyinfo and pagetypeinfo fixtures"

echo "------------------------------------------"
echo "$FRAG_DESC5"
echo "------------------------------------------"
echo "Command: $MEM_BIN --frag -o /tmp/meminsight -t $FRAG_SMAP_FILE $FRAG_MEMINFO_FILE"

rm -rf /tmp/meminsight/*.csv

if $MEM_BIN --frag -o /tmp/meminsight -t "$FRAG_SMAP_FILE" "$FRAG_MEMINFO_FILE"; then
    CSV_FILE=$(ls /tmp/meminsight/*.csv 2>/dev/null | head -n 1)
    if [ -n "$CSV_FILE" ] && \
       grep -F "Fragmentation:" "$CSV_FILE" >/dev/null 2>&1 && \
       grep -F "parse_status,source_unavailable" "$CSV_FILE" >/dev/null 2>&1 && \
       ! grep -F "Fragmentation_PagetypeInfo:" "$CSV_FILE" >/dev/null 2>&1 && \
       ! grep -F "Fragmentation_BuddyInfo:" "$CSV_FILE" >/dev/null 2>&1; then
        echo "✓ $FRAG_DESC5 PASSED"
    else
        echo "✗ $FRAG_DESC5 FAILED (expected source_unavailable fragmentation section missing/invalid)"
        [ -n "$CSV_FILE" ] && cat "$CSV_FILE"
        TEST_FAILED=$((TEST_FAILED + 1))
    fi
else
    echo "✗ $FRAG_DESC5 FAILED (command execution failed)"
    TEST_FAILED=$((TEST_FAILED + 1))
fi
echo ""

# JSON output test (runs only when JSON support is compiled in)
JSON_DESC="Test 13: JSON output includes expected top-level keys"
JSON_SMAP_FILE="test/1-non-zero-swap-entry/meminsight_testSmap.txt"
JSON_MEMINFO_FILE="test/1-non-zero-swap-entry/meminsight_testMeminfo.txt"
JSON_BUDDY_FILE="test/6-buddyinfo-sample/meminsight_testBuddyinfo.txt"
JSON_PGT_FILE="test/7-pagetypeinfo-sample/meminsight_testPagetypeinfo.txt"

echo "------------------------------------------"
echo "$JSON_DESC"
echo "------------------------------------------"

if $MEM_BIN --help 2>&1 | grep -F -- "--fmt" >/dev/null 2>&1; then
    echo "Command: $MEM_BIN --fmt json --frag -o /tmp/meminsight -t $JSON_SMAP_FILE $JSON_MEMINFO_FILE $JSON_BUDDY_FILE $JSON_PGT_FILE"
    rm -rf /tmp/meminsight/*.json

    if $MEM_BIN --fmt json --frag -o /tmp/meminsight -t "$JSON_SMAP_FILE" "$JSON_MEMINFO_FILE" "$JSON_BUDDY_FILE" "$JSON_PGT_FILE"; then
        JSON_FILE=$(ls /tmp/meminsight/*.json 2>/dev/null | head -n 1)
        if [ -n "$JSON_FILE" ] && [ -f "$JSON_FILE" ] && \
           grep -F '"meminfo"' "$JSON_FILE" >/dev/null 2>&1 && \
           grep -F '"processes"' "$JSON_FILE" >/dev/null 2>&1 && \
           grep -F '"fragmentation"' "$JSON_FILE" >/dev/null 2>&1; then
            echo "✓ $JSON_DESC PASSED"
        else
            echo "✗ $JSON_DESC FAILED (missing json file or expected keys)"
            [ -n "$JSON_FILE" ] && cat "$JSON_FILE"
            TEST_FAILED=$((TEST_FAILED + 1))
        fi
    else
        echo "✗ $JSON_DESC FAILED (command execution failed)"
        TEST_FAILED=$((TEST_FAILED + 1))
    fi
else
    echo "- $JSON_DESC SKIPPED (JSON support not compiled in this binary)"
fi
echo ""

# Negative test 1: intentionally malformed meminfo fixture (duplicate needed field)
NEG_DESC="Test 8 (Negative): meminfo data failure"
NEG_SMAP_FILE="test/1-non-zero-swap-entry/meminsight_testSmap.txt"
NEG_MEMINFO_FILE="test/4-negative-duplicate-meminfo-field/meminsight_testMeminfo.txt"
NEG_LOG_FILE="/tmp/meminsight_negative_test.log"

echo "------------------------------------------"
echo "$NEG_DESC"
echo "------------------------------------------"
echo "Command: $MEM_BIN -o /tmp/meminsight -t $NEG_SMAP_FILE $NEG_MEMINFO_FILE"

rm -rf /tmp/meminsight/*.csv

$MEM_BIN -o /tmp/meminsight -t "$NEG_SMAP_FILE" "$NEG_MEMINFO_FILE" >"$NEG_LOG_FILE" 2>&1
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

# Negative test 2: duplicate smap field triggers failure
NEG_DESC2="Test 9 (Negative): smap data failure"
NEG_SMAP_FILE2="test/5-negative-duplicate-smaps-field/meminsight_testSmap.txt"
NEG_MEMINFO_FILE2="test/1-non-zero-swap-entry/meminsight_testMeminfo.txt"
NEG_LOG_FILE2="/tmp/meminsight_negative_test2.log"

echo "------------------------------------------"
echo "$NEG_DESC2"
echo "------------------------------------------"
echo "Command: $MEM_BIN -o /tmp/meminsight -t $NEG_SMAP_FILE2 $NEG_MEMINFO_FILE2"

rm -rf /tmp/meminsight/*.csv

$MEM_BIN -o /tmp/meminsight -t "$NEG_SMAP_FILE2" "$NEG_MEMINFO_FILE2" >"$NEG_LOG_FILE2" 2>&1
RC=$?

if [ "$RC" -eq 0 ]; then
    echo "✗ Negative test 2 FAILED (unexpected success)"
    TEST_FAILED=$((TEST_FAILED + 1))
else
    if grep -F "something went wrong while processing smap for pid" "$NEG_LOG_FILE2" >/dev/null 2>&1; then
        echo "✓ Negative test 2 PASSED (expected failure observed)"
        # cat the log file for visibility
        echo "Log output:"
        cat "$NEG_LOG_FILE2"
    else
        echo "✗ Negative test 2 FAILED (missing expected log line)"
        echo "Log output:"
        cat "$NEG_LOG_FILE2"
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
