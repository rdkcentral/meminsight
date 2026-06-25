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
TC_RESULTS=""

record_tc_result() {
    tc_id="$1"
    tc_summary="$2"
    tc_status="$3"
    if [ -z "$TC_RESULTS" ]; then
        TC_RESULTS="${tc_id}|${tc_summary}|${tc_status}"
    else
        TC_RESULTS=$(printf '%s\n%s|%s|%s' "$TC_RESULTS" "$tc_id" "$tc_summary" "$tc_status")
    fi
}

print_tc_summary_table() {
    printf '+-%-7s-+-%-52s-+-%-15s-+\n' '-------' '----------------------------------------------------' '---------------'
    printf '| %-7s | %-52s | %-15s |\n' "TC ID" "Small summary" "Success/Failure"
    printf '+-%-7s-+-%-52s-+-%-15s-+\n' '-------' '----------------------------------------------------' '---------------'
    printf '%s\n' "$TC_RESULTS" | while IFS='|' read -r tc_id tc_summary tc_status; do
        [ -z "$tc_id" ] && continue
        printf '| %-7.7s | %-52.52s | %-15.15s |\n' "$tc_id" "$tc_summary" "$tc_status"
    done
    printf '+-%-7s-+-%-52s-+-%-15s-+\n' '-------' '----------------------------------------------------' '---------------'
}

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

    TC_STATUS="FAILURE"
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
            TC_STATUS="SUCCESS"
        else
            echo "WARNING: Output CSV file not found in /tmp/meminsight/"
            TEST_FAILED=$((TEST_FAILED + 1))
            TC_STATUS="FAILURE"
        fi
    else
        echo "✗ Test $i FAILED"
        TEST_FAILED=$((TEST_FAILED + 1))
        TC_STATUS="FAILURE"
    fi
    record_tc_result "$i" "$DESC" "$TC_STATUS"
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
        record_tc_result "6" "Fragmentation pagetypeinfo preferred" "SUCCESS"
    else
        echo "✗ $FRAG_DESC1 FAILED (pagetypeinfo section missing)"
        [ -n "$CSV_FILE" ] && cat "$CSV_FILE"
        TEST_FAILED=$((TEST_FAILED + 1))
        record_tc_result "6" "Fragmentation pagetypeinfo preferred" "FAILURE"
    fi
else
    echo "✗ $FRAG_DESC1 FAILED (command execution failed)"
    TEST_FAILED=$((TEST_FAILED + 1))
    record_tc_result "6" "Fragmentation pagetypeinfo preferred" "FAILURE"
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
        record_tc_result "7" "Fragmentation buddyinfo fallback" "SUCCESS"
    else
        echo "✗ $FRAG_DESC2 FAILED (buddyinfo section missing)"
        [ -n "$CSV_FILE" ] && cat "$CSV_FILE"
        TEST_FAILED=$((TEST_FAILED + 1))
        record_tc_result "7" "Fragmentation buddyinfo fallback" "FAILURE"
    fi
else
    echo "✗ $FRAG_DESC2 FAILED (command execution failed)"
    TEST_FAILED=$((TEST_FAILED + 1))
    record_tc_result "7" "Fragmentation buddyinfo fallback" "FAILURE"
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
        record_tc_result "10" "Fragmentation buddyinfo variant format" "SUCCESS"
    else
        echo "✗ $FRAG_DESC3 FAILED (buddyinfo section missing)"
        [ -n "$CSV_FILE" ] && cat "$CSV_FILE"
        TEST_FAILED=$((TEST_FAILED + 1))
        record_tc_result "10" "Fragmentation buddyinfo variant format" "FAILURE"
    fi
else
    echo "✗ $FRAG_DESC3 FAILED (command execution failed)"
    TEST_FAILED=$((TEST_FAILED + 1))
    record_tc_result "10" "Fragmentation buddyinfo variant format" "FAILURE"
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
        record_tc_result "11" "Fragmentation pagetypeinfo variant format" "SUCCESS"
    else
        echo "✗ $FRAG_DESC4 FAILED (pagetypeinfo section missing)"
        [ -n "$CSV_FILE" ] && cat "$CSV_FILE"
        TEST_FAILED=$((TEST_FAILED + 1))
        record_tc_result "11" "Fragmentation pagetypeinfo variant format" "FAILURE"
    fi
else
    echo "✗ $FRAG_DESC4 FAILED (command execution failed)"
    TEST_FAILED=$((TEST_FAILED + 1))
    record_tc_result "11" "Fragmentation pagetypeinfo variant format" "FAILURE"
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
        record_tc_result "12" "Fragmentation source missing fault-injection" "SUCCESS"
    else
        echo "✗ $FRAG_DESC5 FAILED (expected source_unavailable fragmentation section missing/invalid)"
        [ -n "$CSV_FILE" ] && cat "$CSV_FILE"
        TEST_FAILED=$((TEST_FAILED + 1))
        record_tc_result "12" "Fragmentation source missing fault-injection" "FAILURE"
    fi
else
    echo "✗ $FRAG_DESC5 FAILED (command execution failed)"
    TEST_FAILED=$((TEST_FAILED + 1))
    record_tc_result "12" "Fragmentation source missing fault-injection" "FAILURE"
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
           grep -F '"cpustat"' "$JSON_FILE" >/dev/null 2>&1 && \
           grep -F '"processes"' "$JSON_FILE" >/dev/null 2>&1 && \
           grep -F '"fragmentation"' "$JSON_FILE" >/dev/null 2>&1; then
            echo "✓ $JSON_DESC PASSED"
            record_tc_result "13" "JSON top-level keys" "SUCCESS"
        else
            echo "✗ $JSON_DESC FAILED (missing json file or expected keys)"
            [ -n "$JSON_FILE" ] && cat "$JSON_FILE"
            TEST_FAILED=$((TEST_FAILED + 1))
            record_tc_result "13" "JSON top-level keys" "FAILURE"
        fi
    else
        echo "✗ $JSON_DESC FAILED (command execution failed)"
        TEST_FAILED=$((TEST_FAILED + 1))
        record_tc_result "13" "JSON top-level keys" "FAILURE"
    fi
else
    echo "- $JSON_DESC SKIPPED (JSON support not compiled in this binary)"
    record_tc_result "13" "JSON top-level keys" "SKIPPED"
fi
echo ""

# Device property fallback test: invalid interface in /etc/device.properties should yield DEFAULT_MAC
DEVPROP_DESC="Test 14: device property invalid interface falls back to DEFAULT_MAC"
DEVPROP_FILE="/etc/device.properties"
DEVPROP_BAK="/tmp/meminsight_device_properties.bak"
DEVPROP_LOG="/tmp/meminsight_deviceprop_test.log"
DEVPROP_SMAP_FILE="test/1-non-zero-swap-entry/meminsight_testSmap.txt"
DEVPROP_MEMINFO_FILE="test/1-non-zero-swap-entry/meminsight_testMeminfo.txt"

echo "------------------------------------------"
echo "$DEVPROP_DESC"
echo "------------------------------------------"

if [ -w "/etc" ] || [ -w "$DEVPROP_FILE" ] || [ ! -e "$DEVPROP_FILE" ]; then
    if [ -f "$DEVPROP_FILE" ]; then
        cp "$DEVPROP_FILE" "$DEVPROP_BAK"
    else
        rm -f "$DEVPROP_BAK"
    fi

    # Force lookup of a non-existent interface so MAC resolution must fallback.
    printf 'ESTB_INTERFACE=meminsight_invalid_if\n' > "$DEVPROP_FILE"

    rm -rf /tmp/meminsight/*.csv
    if $MEM_BIN -o /tmp/meminsight -t "$DEVPROP_SMAP_FILE" "$DEVPROP_MEMINFO_FILE" >"$DEVPROP_LOG" 2>&1; then
        CSV_FILE=$(ls /tmp/meminsight/*.csv 2>/dev/null | head -n 1)
        if [ -n "$CSV_FILE" ] && grep -E '^[^,]+,00:00:00:00:00:00,' "$CSV_FILE" >/dev/null 2>&1; then
            echo "✓ $DEVPROP_DESC PASSED"
            record_tc_result "14" "Invalid interface -> DEFAULT_MAC" "SUCCESS"
        else
            echo "✗ $DEVPROP_DESC FAILED (DEFAULT_MAC not found in metadata row)"
            [ -n "$CSV_FILE" ] && cat "$CSV_FILE"
            TEST_FAILED=$((TEST_FAILED + 1))
            record_tc_result "14" "Invalid interface -> DEFAULT_MAC" "FAILURE"
        fi
    else
        echo "✗ $DEVPROP_DESC FAILED (command execution failed)"
        cat "$DEVPROP_LOG"
        TEST_FAILED=$((TEST_FAILED + 1))
        record_tc_result "14" "Invalid interface -> DEFAULT_MAC" "FAILURE"
    fi

    # Restore device properties state
    if [ -f "$DEVPROP_BAK" ]; then
        mv "$DEVPROP_BAK" "$DEVPROP_FILE"
    else
        rm -f "$DEVPROP_FILE"
    fi
else
    echo "- $DEVPROP_DESC SKIPPED (/etc/device.properties not writable in this environment)"
    record_tc_result "14" "Invalid interface -> DEFAULT_MAC" "SKIPPED"
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
    record_tc_result "8" "Negative meminfo data failure" "FAILURE"
else
    if grep -F "Test Failed..meminfoHeader vs tstmeminfoHeader" "$NEG_LOG_FILE" >/dev/null 2>&1; then
        echo "✓ Negative test PASSED (expected failure observed)"
        # cat the log file for visibility
        echo "Log output:"
        cat "$NEG_LOG_FILE"
        record_tc_result "8" "Negative meminfo data failure" "SUCCESS"
    else
        echo "✗ Negative test FAILED (missing expected log line)"
        echo "Log output:"
        cat "$NEG_LOG_FILE"
        TEST_FAILED=$((TEST_FAILED + 1))
        record_tc_result "8" "Negative meminfo data failure" "FAILURE"
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
    record_tc_result "9" "Negative smap data failure" "FAILURE"
else
    if grep -F "something went wrong while processing smap for pid" "$NEG_LOG_FILE2" >/dev/null 2>&1; then
        echo "✓ Negative test 2 PASSED (expected failure observed)"
        # cat the log file for visibility
        echo "Log output:"
        cat "$NEG_LOG_FILE2"
        record_tc_result "9" "Negative smap data failure" "SUCCESS"
    else
        echo "✗ Negative test 2 FAILED (missing expected log line)"
        echo "Log output:"
        cat "$NEG_LOG_FILE2"
        TEST_FAILED=$((TEST_FAILED + 1))
        record_tc_result "9" "Negative smap data failure" "FAILURE"
    fi
fi
echo ""

# CPU stat section test using deterministic /proc/stat fixture
CPU_DESC="Test 15: CPU stat raw section"
CPU_SMAP_FILE="test/1-non-zero-swap-entry/meminsight_testSmap.txt"
CPU_MEMINFO_FILE="test/1-non-zero-swap-entry/meminsight_testMeminfo.txt"
CPU_BUDDY_FILE="test/6-buddyinfo-sample/meminsight_testBuddyinfo.txt"
CPU_PGT_FILE="test/7-pagetypeinfo-sample/meminsight_testPagetypeinfo.txt"
CPU_STAT_FILE="test/10-cpu-stat-sample/meminsight_testStat.txt"

echo "------------------------------------------"
echo "$CPU_DESC"
echo "------------------------------------------"
echo "Command: $MEM_BIN -o /tmp/meminsight -t $CPU_SMAP_FILE $CPU_MEMINFO_FILE $CPU_BUDDY_FILE $CPU_PGT_FILE $CPU_STAT_FILE"

rm -rf /tmp/meminsight/*.csv

if $MEM_BIN -o /tmp/meminsight -t "$CPU_SMAP_FILE" "$CPU_MEMINFO_FILE" "$CPU_BUDDY_FILE" "$CPU_PGT_FILE" "$CPU_STAT_FILE"; then
    CSV_FILE=$(ls /tmp/meminsight/*.csv 2>/dev/null | head -n 1)
    if [ -n "$CSV_FILE" ] && \
       grep -F "CPUStat:" "$CSV_FILE" >/dev/null 2>&1 && \
       grep -F "USER,NICE,SYSTEM,IDLE,IOWAIT,IRQ,SOFTIRQ,STEAL,GUEST,GUEST_NICE" "$CSV_FILE" >/dev/null 2>&1 && \
       grep -F "30543,1668,49363,252716,1758,0,3528,0,0,0" "$CSV_FILE" >/dev/null 2>&1; then
        echo "✓ $CPU_DESC PASSED"
        record_tc_result "15" "CPU stat raw section" "SUCCESS"
    else
        echo "✗ $CPU_DESC FAILED (CPU section/header/value missing)"
        [ -n "$CSV_FILE" ] && cat "$CSV_FILE"
        TEST_FAILED=$((TEST_FAILED + 1))
        record_tc_result "15" "CPU stat raw section" "FAILURE"
    fi
else
    echo "✗ $CPU_DESC FAILED (command execution failed)"
    TEST_FAILED=$((TEST_FAILED + 1))
    record_tc_result "15" "CPU stat raw section" "FAILURE"
fi

# Also validate JSON cpu_stat emission when JSON support is available
if $MEM_BIN --help 2>&1 | grep -F -- "--fmt" >/dev/null 2>&1; then
    rm -rf /tmp/meminsight/*.json /tmp/meminsight/*.csv
    if $MEM_BIN --fmt json -o /tmp/meminsight -t "$CPU_SMAP_FILE" "$CPU_MEMINFO_FILE" "$CPU_BUDDY_FILE" "$CPU_PGT_FILE" "$CPU_STAT_FILE"; then
        JSON_FILE=$(ls /tmp/meminsight/*.json 2>/dev/null | head -n 1)
        CSV_FALLBACK_FILE=$(ls /tmp/meminsight/*.csv 2>/dev/null | head -n 1)
        if [ -n "$JSON_FILE" ] && \
           grep -F '"cpu_stat"' "$JSON_FILE" >/dev/null 2>&1 && \
           grep -Eq '"user"[[:space:]]*:[[:space:]]*30543(\.0+)?' "$JSON_FILE" >/dev/null 2>&1; then
            echo "✓ $CPU_DESC (JSON cpu_stat) PASSED"
            record_tc_result "15-json" "CPU stat JSON section" "SUCCESS"
        elif [ -z "$JSON_FILE" ] && [ -n "$CSV_FALLBACK_FILE" ]; then
            echo "- $CPU_DESC (JSON cpu_stat) SKIPPED (runtime cJSON unavailable, CSV fallback produced)"
            record_tc_result "15-json" "CPU stat JSON section" "SKIPPED"
        else
            echo "✗ $CPU_DESC (JSON cpu_stat) FAILED (cpu_stat missing/invalid)"
            [ -n "$JSON_FILE" ] && cat "$JSON_FILE"
            TEST_FAILED=$((TEST_FAILED + 1))
            record_tc_result "15-json" "CPU stat JSON section" "FAILURE"
        fi
    else
        echo "✗ $CPU_DESC (JSON cpu_stat) FAILED (command execution failed)"
        TEST_FAILED=$((TEST_FAILED + 1))
        record_tc_result "15-json" "CPU stat JSON section" "FAILURE"
    fi
fi
echo ""

# JSON bandwidth fixture test with deterministic TESTME input
BW_JSON_DESC="Test 16: JSON bandwidth fixture coverage"
BW_STAT_FILE="test/10-cpu-stat-sample/meminsight_testStat.txt"
BW_FIXTURE_FILE="test/12-bandwidth-sample/meminsight_testBandwidth.txt"

if $MEM_BIN --help 2>&1 | grep -F -- "--fmt" >/dev/null 2>&1; then
    echo "------------------------------------------"
    echo "$BW_JSON_DESC"
    echo "------------------------------------------"
    echo "Command: $MEM_BIN --fmt json -o /tmp/meminsight -t $CPU_SMAP_FILE $CPU_MEMINFO_FILE $CPU_BUDDY_FILE $CPU_PGT_FILE $BW_STAT_FILE $BW_FIXTURE_FILE"

    rm -rf /tmp/meminsight/*.json /tmp/meminsight/*.csv

    if $MEM_BIN --fmt json -o /tmp/meminsight -t "$CPU_SMAP_FILE" "$CPU_MEMINFO_FILE" "$CPU_BUDDY_FILE" "$CPU_PGT_FILE" "$BW_STAT_FILE" "$BW_FIXTURE_FILE"; then
        JSON_FILE=$(ls /tmp/meminsight/*.json 2>/dev/null | head -n 1)
        CSV_FALLBACK_FILE=$(ls /tmp/meminsight/*.csv 2>/dev/null | head -n 1)
        if [ -n "$JSON_FILE" ] && \
           grep -F '"bandwidth"' "$JSON_FILE" >/dev/null 2>&1 && \
           grep -Eq '"total_bandwidth"[[:space:]]*:[[:space:]]*123456(\.0+)?' "$JSON_FILE" >/dev/null 2>&1 && \
           grep -Eq '"usage_percentage"[[:space:]]*:[[:space:]]*37\.5(0+)?' "$JSON_FILE" >/dev/null 2>&1; then
            echo "✓ $BW_JSON_DESC PASSED"
            record_tc_result "16" "JSON bandwidth fixture coverage" "SUCCESS"
        elif [ -z "$JSON_FILE" ] && [ -n "$CSV_FALLBACK_FILE" ]; then
            echo "- $BW_JSON_DESC SKIPPED (runtime cJSON unavailable, CSV fallback produced)"
            record_tc_result "16" "JSON bandwidth fixture coverage" "SKIPPED"
        else
            echo "✗ $BW_JSON_DESC FAILED (bandwidth object missing/invalid)"
            [ -n "$JSON_FILE" ] && cat "$JSON_FILE"
            TEST_FAILED=$((TEST_FAILED + 1))
            record_tc_result "16" "JSON bandwidth fixture coverage" "FAILURE"
        fi
    else
        echo "✗ $BW_JSON_DESC FAILED (command execution failed)"
        TEST_FAILED=$((TEST_FAILED + 1))
        record_tc_result "16" "JSON bandwidth fixture coverage" "FAILURE"
    fi
    echo ""

    BW_JSON_ABSENT_DESC="Test 17: JSON bandwidth omitted without fixture"
    echo "------------------------------------------"
    echo "$BW_JSON_ABSENT_DESC"
    echo "------------------------------------------"
    echo "Command: $MEM_BIN --fmt json -o /tmp/meminsight -t $CPU_SMAP_FILE $CPU_MEMINFO_FILE $CPU_BUDDY_FILE $CPU_PGT_FILE $BW_STAT_FILE"

    rm -rf /tmp/meminsight/*.json /tmp/meminsight/*.csv

    if $MEM_BIN --fmt json -o /tmp/meminsight -t "$CPU_SMAP_FILE" "$CPU_MEMINFO_FILE" "$CPU_BUDDY_FILE" "$CPU_PGT_FILE" "$BW_STAT_FILE"; then
        JSON_FILE=$(ls /tmp/meminsight/*.json 2>/dev/null | head -n 1)
        CSV_FALLBACK_FILE=$(ls /tmp/meminsight/*.csv 2>/dev/null | head -n 1)
        if [ -n "$JSON_FILE" ] && ! grep -F '"bandwidth"' "$JSON_FILE" >/dev/null 2>&1; then
            echo "✓ $BW_JSON_ABSENT_DESC PASSED"
            record_tc_result "17" "JSON bandwidth omitted without fixture" "SUCCESS"
        elif [ -z "$JSON_FILE" ] && [ -n "$CSV_FALLBACK_FILE" ]; then
            echo "- $BW_JSON_ABSENT_DESC SKIPPED (runtime cJSON unavailable, CSV fallback produced)"
            record_tc_result "17" "JSON bandwidth omitted without fixture" "SKIPPED"
        else
            echo "✗ $BW_JSON_ABSENT_DESC FAILED (unexpected bandwidth object state)"
            [ -n "$JSON_FILE" ] && cat "$JSON_FILE"
            TEST_FAILED=$((TEST_FAILED + 1))
            record_tc_result "17" "JSON bandwidth omitted without fixture" "FAILURE"
        fi
    else
        echo "✗ $BW_JSON_ABSENT_DESC FAILED (command execution failed)"
        TEST_FAILED=$((TEST_FAILED + 1))
        record_tc_result "17" "JSON bandwidth omitted without fixture" "FAILURE"
    fi
    echo ""
fi

# CPU stat compatibility test with legacy aggregate cpu line lacking guest fields
CPU_COMPAT_DESC="Test 18: CPU stat legacy field-count compatibility"
CPU_COMPAT_STAT_FILE="test/11-cpu-stat-legacy-fields/meminsight_testStat.txt"

echo "------------------------------------------"
echo "$CPU_COMPAT_DESC"
echo "------------------------------------------"
echo "Command: $MEM_BIN -o /tmp/meminsight -t $CPU_SMAP_FILE $CPU_MEMINFO_FILE $CPU_BUDDY_FILE $CPU_PGT_FILE $CPU_COMPAT_STAT_FILE"

rm -rf /tmp/meminsight/*.csv

if $MEM_BIN -o /tmp/meminsight -t "$CPU_SMAP_FILE" "$CPU_MEMINFO_FILE" "$CPU_BUDDY_FILE" "$CPU_PGT_FILE" "$CPU_COMPAT_STAT_FILE"; then
    CSV_FILE=$(ls /tmp/meminsight/*.csv 2>/dev/null | head -n 1)
    if [ -n "$CSV_FILE" ] && \
       grep -F "CPUStat:" "$CSV_FILE" >/dev/null 2>&1 && \
       grep -F "30543,1668,49363,252716,1758,0,3528,0,0,0" "$CSV_FILE" >/dev/null 2>&1; then
        echo "✓ $CPU_COMPAT_DESC PASSED"
        record_tc_result "18" "CPU stat legacy field-count compatibility" "SUCCESS"
    else
        echo "✗ $CPU_COMPAT_DESC FAILED (legacy CPUStat row missing/invalid)"
        [ -n "$CSV_FILE" ] && cat "$CSV_FILE"
        TEST_FAILED=$((TEST_FAILED + 1))
        record_tc_result "18" "CPU stat legacy field-count compatibility" "FAILURE"
    fi
else
    echo "✗ $CPU_COMPAT_DESC FAILED (command execution failed)"
    TEST_FAILED=$((TEST_FAILED + 1))
    record_tc_result "18" "CPU stat legacy field-count compatibility" "FAILURE"
fi
echo ""

# Backup policy tests
RET_SMAP_FILE="test/1-non-zero-swap-entry/meminsight_testSmap.txt"
RET_MEMINFO_FILE="test/1-non-zero-swap-entry/meminsight_testMeminfo.txt"
RET_BASE="/tmp/meminsight_backup_tests"
BACKUP_BASE="backup"
rm -rf "$RET_BASE"
mkdir -p "$RET_BASE"

# Test 19: output dir missing -> create and proceed
RET_DESC1="Test 19: Backup create missing output directory"
RET_OUT1="$RET_BASE/meminsight_case1_missing_dir"
RET_OUT1_BASENAME="$(basename "$RET_OUT1")"

echo "------------------------------------------"
echo "$RET_DESC1"
echo "------------------------------------------"
echo "Command: $MEM_BIN -o $RET_OUT1 -t $RET_SMAP_FILE $RET_MEMINFO_FILE"

rm -rf "$RET_OUT1"
rm -f /tmp/.meminsight_configstore
if $MEM_BIN -o "$RET_OUT1" -t "$RET_SMAP_FILE" "$RET_MEMINFO_FILE"; then
    if [ -d "$RET_OUT1" ] && [ -f "$RET_OUT1/.meminsight_configstore" ] && ls "$RET_OUT1"/*.csv >/dev/null 2>&1 && [ ! -e /tmp/.meminsight_configstore ]; then
        echo "✓ $RET_DESC1 PASSED"
        record_tc_result "19" "Backup create missing output directory" "SUCCESS"
    else
        echo "✗ $RET_DESC1 FAILED (output directory/report/configstore not created as expected)"
        TEST_FAILED=$((TEST_FAILED + 1))
        record_tc_result "19" "Backup create missing output directory" "FAILURE"
    fi
else
    echo "✗ $RET_DESC1 FAILED (command execution failed)"
    TEST_FAILED=$((TEST_FAILED + 1))
    record_tc_result "19" "Backup create missing output directory" "FAILURE"
fi
echo ""

# Test 20: output dir exists and empty -> proceed without creating backup dir
RET_DESC2="Test 20: Backup existing empty output directory"
RET_OUT2="$RET_BASE/meminsight_case2_empty_dir"
RET_OUT2_BASENAME="$(basename "$RET_OUT2")"

echo "------------------------------------------"
echo "$RET_DESC2"
echo "------------------------------------------"
echo "Command: $MEM_BIN -o $RET_OUT2 -t $RET_SMAP_FILE $RET_MEMINFO_FILE"

rm -rf "$RET_OUT2"
mkdir -p "$RET_OUT2"
if $MEM_BIN -o "$RET_OUT2" -t "$RET_SMAP_FILE" "$RET_MEMINFO_FILE"; then
    RET_DIR_COUNT=$(find "$RET_OUT2" -maxdepth 1 -type d -name "*_${BACKUP_BASE}*" | wc -l | tr -d ' ')
    if [ "$RET_DIR_COUNT" -eq 0 ] && ls "$RET_OUT2"/*.csv >/dev/null 2>&1; then
        echo "✓ $RET_DESC2 PASSED"
        record_tc_result "20" "Backup existing empty output directory" "SUCCESS"
    else
        echo "✗ $RET_DESC2 FAILED (unexpected backup dir or missing report)"
        find "$RET_OUT2" -maxdepth 2 -print
        TEST_FAILED=$((TEST_FAILED + 1))
        record_tc_result "20" "Backup existing empty output directory" "FAILURE"
    fi
else
    echo "✗ $RET_DESC2 FAILED (command execution failed)"
    TEST_FAILED=$((TEST_FAILED + 1))
    record_tc_result "20" "Backup existing empty output directory" "FAILURE"
fi
echo ""

# Test 21: report count <= configured count -> move all matching reports into backup dir
RET_DESC3="Test 21: Backup move-all when count <= N (CSV)"
RET_OUT3="$RET_BASE/meminsight_case3_le_backup"
RET_OUT3_BASENAME="$(basename "$RET_OUT3")"

echo "------------------------------------------"
echo "$RET_DESC3"
echo "------------------------------------------"
echo "Command: $MEM_BIN -o $RET_OUT3 -b 5 -t $RET_SMAP_FILE $RET_MEMINFO_FILE"

rm -rf "$RET_OUT3"
mkdir -p "$RET_OUT3"
printf 'old1\n' > "$RET_OUT3/old_1.csv"
printf 'old2\n' > "$RET_OUT3/old_2.csv"

if $MEM_BIN -o "$RET_OUT3" -b 5 -t "$RET_SMAP_FILE" "$RET_MEMINFO_FILE"; then
    RET_DIR=$(find "$RET_OUT3" -mindepth 1 -maxdepth 1 -type d -name "*_${BACKUP_BASE}*" | head -n 1)
    ARCHIVED_OLD_COUNT=0
    ROOT_OLD_COUNT=0
    ROOT_NEW_REPORT_COUNT=0
    if [ -n "$RET_DIR" ]; then
        ARCHIVED_OLD_COUNT=$(find "$RET_DIR" -maxdepth 1 -type f -name 'old_*.csv' | wc -l | tr -d ' ')
    fi
    ROOT_OLD_COUNT=$(find "$RET_OUT3" -maxdepth 1 -type f -name 'old_*.csv' | wc -l | tr -d ' ')
    ROOT_NEW_REPORT_COUNT=$(find "$RET_OUT3" -maxdepth 1 -type f -name '*_meminsight.csv' | wc -l | tr -d ' ')
    if [ -n "$RET_DIR" ] && [ "$ARCHIVED_OLD_COUNT" -eq 2 ] && [ "$ROOT_OLD_COUNT" -eq 0 ] && [ "$ROOT_NEW_REPORT_COUNT" -ge 1 ]; then
        echo "✓ $RET_DESC3 PASSED"
        record_tc_result "21" "Backup move-all when count <= N (CSV)" "SUCCESS"
    else
        echo "✗ $RET_DESC3 FAILED (old csv files not moved/archived as expected)"
        find "$RET_OUT3" -maxdepth 2 -print
        TEST_FAILED=$((TEST_FAILED + 1))
        record_tc_result "21" "Backup move-all when count <= N (CSV)" "FAILURE"
    fi
else
    echo "✗ $RET_DESC3 FAILED (command execution failed)"
    TEST_FAILED=$((TEST_FAILED + 1))
    record_tc_result "21" "Backup move-all when count <= N (CSV)" "FAILURE"
fi
echo ""

# Test 22: report count > configured count -> move newest N and delete older matching reports
RET_DESC4="Test 22: Backup move-latest-N and delete rest (CSV)"
RET_OUT4="$RET_BASE/meminsight_case4_gt_backup"
RET_OUT4_BASENAME="$(basename "$RET_OUT4")"

echo "------------------------------------------"
echo "$RET_DESC4"
echo "------------------------------------------"
echo "Command: $MEM_BIN -o $RET_OUT4 -b 3 -t $RET_SMAP_FILE $RET_MEMINFO_FILE"

rm -rf "$RET_OUT4"
mkdir -p "$RET_OUT4"
printf 'old1\n' > "$RET_OUT4/old_1.csv"
printf 'old2\n' > "$RET_OUT4/old_2.csv"
printf 'old3\n' > "$RET_OUT4/old_3.csv"
printf 'old4\n' > "$RET_OUT4/old_4.csv"
printf 'old5\n' > "$RET_OUT4/old_5.csv"
touch -t 202601010101 "$RET_OUT4/old_1.csv"
touch -t 202601010102 "$RET_OUT4/old_2.csv"
touch -t 202601010103 "$RET_OUT4/old_3.csv"
touch -t 202601010104 "$RET_OUT4/old_4.csv"
touch -t 202601010105 "$RET_OUT4/old_5.csv"

if $MEM_BIN -o "$RET_OUT4" -b 3 -t "$RET_SMAP_FILE" "$RET_MEMINFO_FILE"; then
    RET_DIR=$(find "$RET_OUT4" -mindepth 1 -maxdepth 1 -type d -name "*_${BACKUP_BASE}*" | head -n 1)
    ROOT_OLD_COUNT=$(find "$RET_OUT4" -maxdepth 1 -type f -name 'old_*.csv' | wc -l | tr -d ' ')
    ROOT_NEW_REPORT_COUNT=$(find "$RET_OUT4" -maxdepth 1 -type f -name '*_meminsight.csv' | wc -l | tr -d ' ')
    if [ -n "$RET_DIR" ] && [ -f "$RET_DIR/old_3.csv" ] && [ -f "$RET_DIR/old_4.csv" ] && [ -f "$RET_DIR/old_5.csv" ] && [ ! -f "$RET_DIR/old_1.csv" ] && [ ! -f "$RET_DIR/old_2.csv" ] && [ "$ROOT_OLD_COUNT" -eq 0 ] && [ "$ROOT_NEW_REPORT_COUNT" -ge 1 ]; then
        echo "✓ $RET_DESC4 PASSED"
        record_tc_result "22" "Backup move-latest-N and delete rest (CSV)" "SUCCESS"
    else
        echo "✗ $RET_DESC4 FAILED (backup latest-N/delete behavior mismatch)"
        find "$RET_OUT4" -maxdepth 2 -print
        TEST_FAILED=$((TEST_FAILED + 1))
        record_tc_result "22" "Backup move-latest-N and delete rest (CSV)" "FAILURE"
    fi
else
    echo "✗ $RET_DESC4 FAILED (command execution failed)"
    TEST_FAILED=$((TEST_FAILED + 1))
    record_tc_result "22" "Backup move-latest-N and delete rest (CSV)" "FAILURE"
fi
echo ""

# Test 23: format scoping (CSV mode should ignore JSON files)
RET_DESC5="Test 23: Backup format scoping in CSV mode"
RET_OUT5="$RET_BASE/meminsight_case5_csv_scope"
RET_OUT5_BASENAME="$(basename "$RET_OUT5")"

echo "------------------------------------------"
echo "$RET_DESC5"
echo "------------------------------------------"
echo "Command: $MEM_BIN -o $RET_OUT5 -b 10 -t $RET_SMAP_FILE $RET_MEMINFO_FILE"

rm -rf "$RET_OUT5"
mkdir -p "$RET_OUT5"
printf 'csvold\n' > "$RET_OUT5/only_csv.csv"
printf '{"old":1}\n' > "$RET_OUT5/only_json.json"

if $MEM_BIN -o "$RET_OUT5" -b 10 -t "$RET_SMAP_FILE" "$RET_MEMINFO_FILE"; then
    RET_DIR=$(find "$RET_OUT5" -mindepth 1 -maxdepth 1 -type d -name "*_${BACKUP_BASE}*" | head -n 1)
    if [ -n "$RET_DIR" ] && [ -f "$RET_DIR/only_csv.csv" ] && [ -f "$RET_OUT5/only_json.json" ]; then
        echo "✓ $RET_DESC5 PASSED"
        record_tc_result "23" "Backup format scoping in CSV mode" "SUCCESS"
    else
        echo "✗ $RET_DESC5 FAILED (CSV mode backup scope mismatch)"
        find "$RET_OUT5" -maxdepth 2 -print
        TEST_FAILED=$((TEST_FAILED + 1))
        record_tc_result "23" "Backup format scoping in CSV mode" "FAILURE"
    fi
else
    echo "✗ $RET_DESC5 FAILED (command execution failed)"
    TEST_FAILED=$((TEST_FAILED + 1))
    record_tc_result "23" "Backup format scoping in CSV mode" "FAILURE"
fi
echo ""

# Test 24: format scoping (JSON mode should ignore CSV files)
RET_DESC6="Test 24: Backup format scoping in JSON mode"
RET_OUT6="$RET_BASE/meminsight_case6_json_scope"
RET_OUT6_BASENAME="$(basename "$RET_OUT6")"

if $MEM_BIN --help 2>&1 | grep -F -- "--fmt" >/dev/null 2>&1; then
    echo "------------------------------------------"
    echo "$RET_DESC6"
    echo "------------------------------------------"
    echo "Command: $MEM_BIN --fmt json -o $RET_OUT6 -b 10 -t $RET_SMAP_FILE $RET_MEMINFO_FILE"

    rm -rf "$RET_OUT6"
    mkdir -p "$RET_OUT6"
    printf 'csvold\n' > "$RET_OUT6/json_scope_csv.csv"
    printf '{"old":2}\n' > "$RET_OUT6/json_scope_json.json"

    if $MEM_BIN --fmt json -o "$RET_OUT6" -b 10 -t "$RET_SMAP_FILE" "$RET_MEMINFO_FILE"; then
        JSON_FILE=$(ls "$RET_OUT6"/*.json 2>/dev/null | grep -v 'json_scope_json.json' | head -n 1)
        CSV_FALLBACK_FILE=$(ls "$RET_OUT6"/*.csv 2>/dev/null | grep -v 'json_scope_csv.csv' | head -n 1)
        if [ -n "$JSON_FILE" ]; then
            RET_DIR=$(find "$RET_OUT6" -mindepth 1 -maxdepth 1 -type d -name "*_${BACKUP_BASE}*" | head -n 1)
            if [ -n "$RET_DIR" ] && [ -f "$RET_DIR/json_scope_json.json" ] && [ -f "$RET_OUT6/json_scope_csv.csv" ]; then
                echo "✓ $RET_DESC6 PASSED"
                record_tc_result "24" "Backup format scoping in JSON mode" "SUCCESS"
            else
                echo "✗ $RET_DESC6 FAILED (JSON mode backup scope mismatch)"
                find "$RET_OUT6" -maxdepth 2 -print
                TEST_FAILED=$((TEST_FAILED + 1))
                record_tc_result "24" "Backup format scoping in JSON mode" "FAILURE"
            fi
        elif [ -n "$CSV_FALLBACK_FILE" ]; then
            echo "- $RET_DESC6 SKIPPED (runtime cJSON unavailable, CSV fallback produced)"
            record_tc_result "24" "Backup format scoping in JSON mode" "SKIPPED"
        else
            echo "✗ $RET_DESC6 FAILED (no JSON output and no CSV fallback detected)"
            TEST_FAILED=$((TEST_FAILED + 1))
            record_tc_result "24" "Backup format scoping in JSON mode" "FAILURE"
        fi
    else
        echo "✗ $RET_DESC6 FAILED (command execution failed)"
        TEST_FAILED=$((TEST_FAILED + 1))
        record_tc_result "24" "Backup format scoping in JSON mode" "FAILURE"
    fi
    echo ""
else
    record_tc_result "24" "Backup format scoping in JSON mode" "SKIPPED"
fi

# Test 25: CSV metadata includes backup fields when --backup is passed
META_DESC1="Test 25: CSV backup metadata with explicit --backup"
META_OUT1="$RET_BASE/meminsight_case7_csv_meta_explicit"

echo "------------------------------------------"
echo "$META_DESC1"
echo "------------------------------------------"
echo "Command: $MEM_BIN -o $META_OUT1 -b 42 -t $RET_SMAP_FILE $RET_MEMINFO_FILE"

rm -rf "$META_OUT1"
mkdir -p "$META_OUT1"

if $MEM_BIN -o "$META_OUT1" -b 42 -t "$RET_SMAP_FILE" "$RET_MEMINFO_FILE"; then
    CSV_FILE=$(ls "$META_OUT1"/*.csv 2>/dev/null | head -n 1)
    if [ -n "$CSV_FILE" ] && \
       head -n 1 "$CSV_FILE" | grep -F "BACKUP_ARG_PASSED,BACKUP_COUNT" >/dev/null 2>&1 && \
       sed -n '2p' "$CSV_FILE" | grep -E ',1,42$' >/dev/null 2>&1; then
        echo "✓ $META_DESC1 PASSED"
        record_tc_result "25" "CSV backup metadata with explicit --backup" "SUCCESS"
    else
        echo "✗ $META_DESC1 FAILED (CSV backup metadata missing/invalid)"
        [ -n "$CSV_FILE" ] && cat "$CSV_FILE"
        TEST_FAILED=$((TEST_FAILED + 1))
        record_tc_result "25" "CSV backup metadata with explicit --backup" "FAILURE"
    fi
else
    echo "✗ $META_DESC1 FAILED (command execution failed)"
    TEST_FAILED=$((TEST_FAILED + 1))
    record_tc_result "25" "CSV backup metadata with explicit --backup" "FAILURE"
fi
echo ""

# Test 26: CSV metadata includes default backup fields when --backup is not passed
META_DESC2="Test 26: CSV backup metadata with default backup"
META_OUT2="$RET_BASE/meminsight_case8_csv_meta_default"

echo "------------------------------------------"
echo "$META_DESC2"
echo "------------------------------------------"
echo "Command: $MEM_BIN -o $META_OUT2 -t $RET_SMAP_FILE $RET_MEMINFO_FILE"

rm -rf "$META_OUT2"
mkdir -p "$META_OUT2"

if $MEM_BIN -o "$META_OUT2" -t "$RET_SMAP_FILE" "$RET_MEMINFO_FILE"; then
    CSV_FILE=$(ls "$META_OUT2"/*.csv 2>/dev/null | head -n 1)
    if [ -n "$CSV_FILE" ] && \
       head -n 1 "$CSV_FILE" | grep -F "BACKUP_ARG_PASSED,BACKUP_COUNT" >/dev/null 2>&1 && \
       sed -n '2p' "$CSV_FILE" | grep -E ',0,30$' >/dev/null 2>&1; then
        echo "✓ $META_DESC2 PASSED"
        record_tc_result "26" "CSV backup metadata with default backup" "SUCCESS"
    else
        echo "✗ $META_DESC2 FAILED (default CSV backup metadata missing/invalid)"
        [ -n "$CSV_FILE" ] && cat "$CSV_FILE"
        TEST_FAILED=$((TEST_FAILED + 1))
        record_tc_result "26" "CSV backup metadata with default backup" "FAILURE"
    fi
else
    echo "✗ $META_DESC2 FAILED (command execution failed)"
    TEST_FAILED=$((TEST_FAILED + 1))
    record_tc_result "26" "CSV backup metadata with default backup" "FAILURE"
fi
echo ""

# Test 27: JSON metadata includes backup fields (skip if CSV fallback)
META_DESC3="Test 27: JSON backup metadata"
META_OUT3="$RET_BASE/meminsight_case9_json_meta"
META_OUT3_BASENAME="$(basename "$META_OUT3")"

if $MEM_BIN --help 2>&1 | grep -F -- "--fmt" >/dev/null 2>&1; then
    echo "------------------------------------------"
    echo "$META_DESC3"
    echo "------------------------------------------"
    echo "Command: $MEM_BIN --fmt json -o $META_OUT3 -b 7 -t $RET_SMAP_FILE $RET_MEMINFO_FILE"

    rm -rf "$META_OUT3"
    mkdir -p "$META_OUT3"

    if $MEM_BIN --fmt json -o "$META_OUT3" -b 7 -t "$RET_SMAP_FILE" "$RET_MEMINFO_FILE"; then
        JSON_FILE=$(ls "$META_OUT3"/*.json 2>/dev/null | head -n 1)
        CSV_FALLBACK_FILE=$(ls "$META_OUT3"/*.csv 2>/dev/null | head -n 1)
        if [ -n "$JSON_FILE" ] && \
              grep -Eq '"BACKUP_ARG_PASSED"[[:space:]]*:[[:space:]]*1' "$JSON_FILE" >/dev/null 2>&1 && \
              grep -Eq '"BACKUP_COUNT"[[:space:]]*:[[:space:]]*7' "$JSON_FILE" >/dev/null 2>&1; then
            echo "✓ $META_DESC3 PASSED"
            record_tc_result "27" "JSON backup metadata" "SUCCESS"
        elif [ -z "$JSON_FILE" ] && [ -n "$CSV_FALLBACK_FILE" ]; then
            echo "- $META_DESC3 SKIPPED (runtime cJSON unavailable, CSV fallback produced)"
            record_tc_result "27" "JSON backup metadata" "SKIPPED"
        else
            echo "✗ $META_DESC3 FAILED (JSON backup metadata missing/invalid)"
            [ -n "$JSON_FILE" ] && cat "$JSON_FILE"
            TEST_FAILED=$((TEST_FAILED + 1))
            record_tc_result "27" "JSON backup metadata" "FAILURE"
        fi
    else
        echo "✗ $META_DESC3 FAILED (command execution failed)"
        TEST_FAILED=$((TEST_FAILED + 1))
        record_tc_result "27" "JSON backup metadata" "FAILURE"
    fi
    echo ""
else
    record_tc_result "27" "JSON backup metadata" "SKIPPED"
fi

# Test 28: configstore lives in output dir and backup archives use prior RUN_ID
META_DESC4="Test 28: Configstore path and RUN_ID-based backup archive"
META_OUT4="$RET_BASE/meminsight_case10_configstore"

echo "------------------------------------------"
echo "$META_DESC4"
echo "------------------------------------------"
echo "Command: $MEM_BIN -o $META_OUT4 -b 3 -t $RET_SMAP_FILE $RET_MEMINFO_FILE"

rm -rf "$META_OUT4"
mkdir -p "$META_OUT4"
rm -f /tmp/.meminsight_configstore

if $MEM_BIN -o "$META_OUT4" -b 3 -t "$RET_SMAP_FILE" "$RET_MEMINFO_FILE"; then
    CONFIG_FILE="$META_OUT4/.meminsight_configstore"
    if [ -f "$CONFIG_FILE" ] && ! [ -e /tmp/.meminsight_configstore ]; then
        FIRST_RUN_ID=$(grep -E '^RUN_ID=' "$CONFIG_FILE" | head -n 1 | cut -d= -f2)
        printf 'old1\n' > "$META_OUT4/old_1.csv"
        printf 'old2\n' > "$META_OUT4/old_2.csv"
        touch -t 202601010101 "$META_OUT4/old_1.csv"
        touch -t 202601010102 "$META_OUT4/old_2.csv"

        if $MEM_BIN -o "$META_OUT4" -b 3 -t "$RET_SMAP_FILE" "$RET_MEMINFO_FILE"; then
            RET_DIR=$(find "$META_OUT4" -maxdepth 1 -type d -name "*_${FIRST_RUN_ID}_${BACKUP_BASE}*" | head -n 1)
            if [ -n "$RET_DIR" ] && [ -f "$RET_DIR/old_1.csv" ] && [ -f "$RET_DIR/old_2.csv" ] && [ -f "$META_OUT4/.meminsight_configstore" ]; then
                echo "✓ $META_DESC4 PASSED"
                record_tc_result "28" "Configstore path and RUN_ID-based backup archive" "SUCCESS"
            else
                echo "✗ $META_DESC4 FAILED (archive name or configstore location mismatch)"
                find "$META_OUT4" -maxdepth 2 -print
                TEST_FAILED=$((TEST_FAILED + 1))
                record_tc_result "28" "Configstore path and RUN_ID-based backup archive" "FAILURE"
            fi
        else
            echo "✗ $META_DESC4 FAILED (second command execution failed)"
            TEST_FAILED=$((TEST_FAILED + 1))
            record_tc_result "28" "Configstore path and RUN_ID-based backup archive" "FAILURE"
        fi
    else
        echo "✗ $META_DESC4 FAILED (configstore not written in output dir or stale /tmp configstore present)"
        TEST_FAILED=$((TEST_FAILED + 1))
        record_tc_result "28" "Configstore path and RUN_ID-based backup archive" "FAILURE"
    fi
else
    echo "✗ $META_DESC4 FAILED (command execution failed)"
    TEST_FAILED=$((TEST_FAILED + 1))
    record_tc_result "28" "Configstore path and RUN_ID-based backup archive" "FAILURE"
fi
echo ""

# Test 29: reject output directory whose basename does not contain meminsight
META_DESC5="Test 29: Reject invalid output directory basename"
META_OUT5="$RET_BASE/backup"
META_LOG5="/tmp/meminsight_invalid_output_dir.log"

echo "------------------------------------------"
echo "$META_DESC5"
echo "------------------------------------------"
echo "Command: $MEM_BIN -o $META_OUT5 -t $RET_SMAP_FILE $RET_MEMINFO_FILE"

rm -rf "$META_OUT5"
if $MEM_BIN -o "$META_OUT5" -t "$RET_SMAP_FILE" "$RET_MEMINFO_FILE" >"$META_LOG5" 2>&1; then
    echo "✗ $META_DESC5 FAILED (command unexpectedly succeeded)"
    cat "$META_LOG5"
    TEST_FAILED=$((TEST_FAILED + 1))
    record_tc_result "29" "Reject invalid output directory basename" "FAILURE"
else
    if grep -F "must have 'meminsight' in the final path component" "$META_LOG5" >/dev/null 2>&1; then
        echo "✓ $META_DESC5 PASSED"
        record_tc_result "29" "Reject invalid output directory basename" "SUCCESS"
    else
        echo "✗ $META_DESC5 FAILED (expected validation message missing)"
        cat "$META_LOG5"
        TEST_FAILED=$((TEST_FAILED + 1))
        record_tc_result "29" "Reject invalid output directory basename" "FAILURE"
    fi
fi
echo ""

# Test 30: upload marker carries upload settings; configstore carries persistent state
META_DESC6="Test 30: Upload marker and configstore key separation"
META_OUT6="$RET_BASE/meminsight_case11_upload_handoff"
META_MARKER="/tmp/.meminsight_upload"

echo "------------------------------------------"
echo "$META_DESC6"
echo "------------------------------------------"
echo "Command: $MEM_BIN --upload-enable --upload-interval 1800 -o $META_OUT6 -t $RET_SMAP_FILE $RET_MEMINFO_FILE"

rm -rf "$META_OUT6"
rm -f "$META_MARKER" /tmp/.meminsight_configstore
if $MEM_BIN --upload-enable --upload-interval 1800 -o "$META_OUT6" -t "$RET_SMAP_FILE" "$RET_MEMINFO_FILE" >/tmp/meminsight_upload_handoff.log 2>&1; then
    META_CONFIG="$META_OUT6/.meminsight_configstore"
    if [ -f "$META_CONFIG" ] && [ -s "$META_MARKER" ] && [ ! -e /tmp/.meminsight_configstore ] && \
       grep -F "RUN_ID=" "$META_CONFIG" >/dev/null 2>&1 && \
       ! grep -E '^(UPLOAD_ENABLED|UPLOAD_INTERVAL)=' "$META_CONFIG" >/dev/null 2>&1 && \
       grep -F "CONFIGSTORE_PATH=$META_CONFIG" "$META_MARKER" >/dev/null 2>&1 && \
       grep -E '^RUN_ID=' "$META_MARKER" >/dev/null 2>&1 && \
       grep -F "UPLOAD_ENABLED=1" "$META_MARKER" >/dev/null 2>&1 && \
       grep -F "UPLOAD_INTERVAL=1800" "$META_MARKER" >/dev/null 2>&1; then
        echo "✓ $META_DESC6 PASSED"
        record_tc_result "30" "Upload marker/configstore key separation" "SUCCESS"
    else
        echo "✗ $META_DESC6 FAILED (marker/configstore ownership mismatch)"
        [ -f "$META_CONFIG" ] && cat "$META_CONFIG"
        [ -f "$META_MARKER" ] && cat "$META_MARKER"
        TEST_FAILED=$((TEST_FAILED + 1))
        record_tc_result "30" "Upload marker/configstore key separation" "FAILURE"
    fi
else
    echo "✗ $META_DESC6 FAILED (command execution failed)"
    cat /tmp/meminsight_upload_handoff.log
    TEST_FAILED=$((TEST_FAILED + 1))
    record_tc_result "30" "Upload marker/configstore key separation" "FAILURE"
fi
rm -f "$META_MARKER"
echo ""

# T2 JSON output test (runs only when JSON support is compiled in)
T2_DESC="Test 16: T2 format produces Report array with nested objects"
T2_SMAP_FILE="test/1-non-zero-swap-entry/meminsight_testSmap.txt"
T2_MEMINFO_FILE="test/1-non-zero-swap-entry/meminsight_testMeminfo.txt"

echo "------------------------------------------"
echo "$T2_DESC"
echo "------------------------------------------"

if $MEM_BIN --help 2>&1 | grep -F "t2" >/dev/null 2>&1; then
    echo "Command: $MEM_BIN --fmt t2 -o /tmp/meminsight -t $T2_SMAP_FILE $T2_MEMINFO_FILE"
    rm -rf /tmp/meminsight/*.t2.json

    if $MEM_BIN --fmt t2 -o /tmp/meminsight -t "$T2_SMAP_FILE" "$T2_MEMINFO_FILE"; then
        T2_FILE=$(ls /tmp/meminsight/*.t2.json 2>/dev/null | head -n 1)
        if [ -n "$T2_FILE" ] && [ -f "$T2_FILE" ] && \
           grep -F '"Report"' "$T2_FILE" >/dev/null 2>&1 && \
           grep -F '"meminfo"' "$T2_FILE" >/dev/null 2>&1 && \
           grep -F '"MemTotal"' "$T2_FILE" >/dev/null 2>&1 && \
           grep -F '"cpu_stats"' "$T2_FILE" >/dev/null 2>&1 && \
           grep -F '"processes"' "$T2_FILE" >/dev/null 2>&1 && \
           grep -F '"mac"' "$T2_FILE" >/dev/null 2>&1; then
            echo "✓ $T2_DESC PASSED"
            echo "Output sample:"
            head -5 "$T2_FILE"
            echo "..."
        else
            echo "✗ $T2_DESC FAILED (missing nested structure)"
            [ -n "$T2_FILE" ] && cat "$T2_FILE"
            TEST_FAILED=$((TEST_FAILED + 1))
        fi
    else
        echo "✗ $T2_DESC FAILED (command execution failed)"
        TEST_FAILED=$((TEST_FAILED + 1))
    fi
else
    echo "- $T2_DESC SKIPPED (T2 format not compiled in this binary)"
fi
echo ""

# Upload URL CLI parsing test
UPLOAD_DESC="Test 17: --upload-url CLI option accepted"
echo "------------------------------------------"
echo "$UPLOAD_DESC"
echo "------------------------------------------"

# Test that --upload-url is accepted without crashing (upload will fail gracefully with no cert/curl)
if $MEM_BIN --help 2>&1 | grep -F "upload-url" >/dev/null 2>&1; then
    UPLOAD_LOG="/tmp/meminsight_upload_test.log"
    echo "Command: $MEM_BIN --fmt t2 --upload-enable --upload-url https://localhost:9999/test -o /tmp/meminsight -t $T2_SMAP_FILE $T2_MEMINFO_FILE"
    rm -rf /tmp/meminsight/*.t2.json

    $MEM_BIN --fmt t2 --upload-enable --upload-url https://localhost:9999/test \
        -o /tmp/meminsight -t "$T2_SMAP_FILE" "$T2_MEMINFO_FILE" >"$UPLOAD_LOG" 2>&1
    RC=$?

    # Binary should still succeed (upload failure doesn't affect exit code)
    if [ "$RC" -eq 0 ] && grep -F "Upload URL" "$UPLOAD_LOG" >/dev/null 2>&1; then
        echo "✓ $UPLOAD_DESC PASSED"
    else
        echo "✗ $UPLOAD_DESC FAILED (exit=$RC)"
        cat "$UPLOAD_LOG"
        TEST_FAILED=$((TEST_FAILED + 1))
    fi
else
    echo "- $UPLOAD_DESC SKIPPED (--upload-url not compiled in this binary)"
fi
echo ""

# T2 format with fragmentation data test
T2_FRAG_DESC="Test 18: T2 format with --frag produces fragmentation object"
T2_FRAG_SMAP_FILE="test/1-non-zero-swap-entry/meminsight_testSmap.txt"
T2_FRAG_MEMINFO_FILE="test/1-non-zero-swap-entry/meminsight_testMeminfo.txt"
T2_FRAG_BUDDY_FILE="test/6-buddyinfo-sample/meminsight_testBuddyinfo.txt"
T2_FRAG_PGT_FILE="test/7-pagetypeinfo-sample/meminsight_testPagetypeinfo.txt"

echo "------------------------------------------"
echo "$T2_FRAG_DESC"
echo "------------------------------------------"

if $MEM_BIN --help 2>&1 | grep -F "t2" >/dev/null 2>&1; then
    echo "Command: $MEM_BIN --fmt t2 --frag -o /tmp/meminsight -t $T2_FRAG_SMAP_FILE $T2_FRAG_MEMINFO_FILE $T2_FRAG_BUDDY_FILE $T2_FRAG_PGT_FILE"
    rm -rf /tmp/meminsight/*.t2.json

    if $MEM_BIN --fmt t2 --frag -o /tmp/meminsight -t "$T2_FRAG_SMAP_FILE" "$T2_FRAG_MEMINFO_FILE" "$T2_FRAG_BUDDY_FILE" "$T2_FRAG_PGT_FILE"; then
        T2_FRAG_FILE=$(ls /tmp/meminsight/*.t2.json 2>/dev/null | head -n 1)
        if [ -n "$T2_FRAG_FILE" ] && [ -f "$T2_FRAG_FILE" ] && \
           grep -F '"fragmentation"' "$T2_FRAG_FILE" >/dev/null 2>&1 && \
           grep -F '"Report"' "$T2_FRAG_FILE" >/dev/null 2>&1; then
            echo "✓ $T2_FRAG_DESC PASSED"
            echo "Output sample:"
            head -10 "$T2_FRAG_FILE"
            echo "..."
        else
            echo "✗ $T2_FRAG_DESC FAILED (missing fragmentation object in T2 output)"
            [ -n "$T2_FRAG_FILE" ] && cat "$T2_FRAG_FILE"
            TEST_FAILED=$((TEST_FAILED + 1))
        fi
    else
        echo "✗ $T2_FRAG_DESC FAILED (command execution failed)"
        TEST_FAILED=$((TEST_FAILED + 1))
    fi
else
    echo "- $T2_FRAG_DESC SKIPPED (T2 format not compiled in this binary)"
fi
echo ""

# Summary
echo "===================================================================================="
echo "Test Summary"
echo "===================================================================================="
print_tc_summary_table
TOTAL_TCS=$(printf '%s\n' "$TC_RESULTS" | awk 'NF {c++} END {print c+0}')
PASSED_TCS=$(printf '%s\n' "$TC_RESULTS" | awk -F'|' '$3=="SUCCESS" {c++} END {print c+0}')
FAILED_TCS=$(printf '%s\n' "$TC_RESULTS" | awk -F'|' '$3=="FAILURE" {c++} END {print c+0}')
SKIPPED_TCS=$(printf '%s\n' "$TC_RESULTS" | awk -F'|' '$3=="SKIPPED" {c++} END {print c+0}')
if [ "$TEST_FAILED" -eq 0 ]; then
    echo "✓ ALL TESTS PASSED | PASSED $PASSED_TCS/$TOTAL_TCS | FAILED $FAILED_TCS/$TOTAL_TCS | SKIPPED $SKIPPED_TCS/$TOTAL_TCS |"
    echo "===================================================================================="
    exit 0
else
    echo "✗ SOME TESTS FAILED | PASSED $PASSED_TCS/$TOTAL_TCS | FAILED $FAILED_TCS/$TOTAL_TCS | SKIPPED $SKIPPED_TCS/$TOTAL_TCS |"
    echo "===================================================================================="
    exit 1
fi
