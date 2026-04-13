# MemInsight Upload Flow - RDK Pattern Integration
**Status:** Design Phase | **Last Updated:** 2026-04-13

---

## 1. HIGH-LEVEL SYSTEM ARCHITECTURE

```
┌─────────────────────────────────────────────────────────────────────┐
│                          RFC CONFIG LAYER                           │
│  (Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.meminsight)        │
└──────────────────┬──────────────────────────────────────────────────┘
                   │ Enable=true (when RFC set by DCM)
                   ▼
┌─────────────────────────────────────────────────────────────────────┐
│                    SYSTEMD PATH TRIGGER LAYER                       │
│  /etc/systemd/system/meminsight-runner.path                         │
│  Monitors: /tmp/.enable_meminsight [create]                         │
└──────────────────┬──────────────────────────────────────────────────┘
                   │ Path condition met
                   ▼
┌─────────────────────────────────────────────────────────────────────┐
│              MEMINSIGHT RUNNER SERVICE (oneshot)                    │
│  /etc/systemd/system/meminsight-runner.service                      │
│  ExecStart: /usr/bin/meminsight --iterations X --interval Y         │
└──────────────────┬──────────────────────────────────────────────────┘
                   │
        ┌──────────┴──────────┐
        │                     │
        ▼                     ▼
   [meminsight.c]      [HOUSEKEEPING]
   MAIN EXECUTION      Touch flags &
   LOOP                 args files
        │                     │
        └──────────┬──────────┘
                   │
        ┌──────────┴──────────────────┐
        │                             │
   Pre-Exec Upload          Per-Iteration Upload Cadence
   (housekeep_0)            (Interval-based trigger)
        │                             │
        └──────────┬──────────────────┘
                   │ Touch /tmp/.upload_memreport
                   ▼
┌─────────────────────────────────────────────────────────────────────┐
│               SYSTEMD UPLOAD PATH TRIGGER LAYER                    │
│  /etc/systemd/system/meminsight-upload.path (monitoring unit)      │
│  Monitors: /tmp/.upload_memreport [create/modify]                  │
└──────────────────┬──────────────────────────────────────────────────┘
                   │ Path condition met → START service
                   ▼
┌─────────────────────────────────────────────────────────────────────┐
│               MEMINSIGHT UPLOAD SERVICE (oneshot)                  │
│  /etc/systemd/system/meminsight-upload.service                     │
│  ExecStart: /opt/rdk/rdk-meminsight/upload_memReports.sh           │
└──────────────────┬──────────────────────────────────────────────────┘
                   │
                   ▼
        ┌──────────────────────────┐
        │  UPLOAD SCRIPT EXECUTION │
        │ (upload_memReports.sh)   │
        │ - Check reports          │
        │ - Tar contents           │
        │ - Fetch upload endpoint  │
        │ - Trigger upload (curl)  │
        │ - Cleanup                │
        └──────────────────────────┘
```

---

## 2. MEMINSIGHT MAIN EXECUTION FLOW

### Phase 1: RFC Enable & Argument Setup

```
┌─────────────────────────────────────┐
│  Sets RFC for MemInsight            │
│  Enable=true                        │
└──────────────────┬──────────────────┘
                   ▼
      ┌────────────────────────┐
      │ touch                  │
      │ /tmp/.enable_meminsight│──────────► Triggers meminsight-runner.path
      └────────────────────────┘
                   │
                   ▼
┌─────────────────────────────────────────────────────────────┐
│            MEMINSIGHT BINARY INVOKED                        │
│  meminsight --iterations 10 --interval 300                 │
│            [+ other args per RFC settings]                 │
└──────────────────┬──────────────────────────────────────────┘
                   │
                   ▼ PHASE: ARGUMENT PARSING & VALIDATION
        ┌──────────────────────────────────────────┐
        │ ✓ Parse all CLI arguments                │
        │ ✓ Validate ranges/values                 │
        │ ✓ Check permissions & paths              │
        │   [If error: log & exit with code]       │
        └──────────────────┬───────────────────────┘
                           │
                           ▼ SUCCESS PATH
        ┌──────────────────────────────────────────┐
        │ Dump parsed arguments to:                │
        │ /tmp/.meminsight_args                    │
        │                                          │
        │ Format (Key=Value pairs):                │
        │   ITERATIONS=10                          │
        │   INTERVAL=300                           │
        │   OUTPUT_DIR=/nvram/meminsight           │
        │   UPLOAD_ON_ITERATION=0|1                │
        │   UPLOAD_ON_COMPLETION=0|1               │
        │   [+ state management args]              │
        └──────────────────┬───────────────────────┘
                           │
                           ▼ PHASE: INITIAL HOUSEKEEPING
        ┌──────────────────────────────────────────┐
        │ touch /tmp/.meminsight_housekeep_0       │
        │  ↓ (signals start of data collection)    │
        │ touch /tmp/.upload_memreport             │
        │  ↓ (triggers upload service for cleanup) │
        │ [Wait for systemd to process triggers]   │
        └──────────────────┬───────────────────────┘
                           │
                           ▼ SUCCESS: Enter iteration loop
```

### Phase 2: Iteration Loop with Cadence Detection

```
┌─────────────────────────────────────────────────────────────┐
│  ITERATION LOOP                                             │
│  for (i=0; i < ITERATIONS; i++)                             │
└──────────────────┬──────────────────────────────────────────┘
                   │
    ┌──────────────┴─────────────────────┐
    │                                    │
    ▼ ITERATION START                    │
┌─────────────────────────────────────┐  │
│ (i) Collect Memory Reports:         │  │
│  • Read /proc/meminfo               │  │
│  • Read /proc/[pid]/smaps/ (all)    │  │
│  • Read /proc/pagetypeinfo          │  │
│  • Read /proc/buddyinfo             │  │
│  • Process memory stats             │  │
│  • Generate CSV/JSON output         │  │
│  • Write to OUTPUT_DIR              │  │
└──────────┬──────────────────────────┘  │
           │                             │
           ▼ WRITE COMPLETE              │
┌─────────────────────────────────────┐  │
│ (ii) Check Upload Cadence:          │  │
│                                     │  │
│  IF (upload_on_iteration == 1)      │  │ LOOP
│    AND (cadence_interval met)       │  │ BACK
│    AND (i < ITERATIONS-1)           │  │ OR
│    THEN:                            │  │ DONE
│    ├─ touch /tmp/.upload_memreport  │  │
│    │  (signals upload service)      │  │
│    └─ log "upload triggered @iter i"│  │
│                                     │  │
│  IF (i < ITERATIONS-1)              │  │
│    THEN:                            │  │
│    ├─ sleep INTERVAL                │──┼──► Return to start
│    └─ continue to next iteration    │  │
│  ELSE:                              │  │
│    └─ break (iteration complete)    │──┴──► Go to Phase 3
└─────────────────────────────────────┘
```

### Phase 3: Post-Execution Cleanup & Final Upload

```
┌─────────────────────────────────────────────────────┐
│  ALL ITERATIONS COMPLETE                            │
└──────────────────┬──────────────────────────────────┘
                   │
                   ▼ PHASE: FINAL HOUSEKEEPING
        ┌──────────────────────────────────────────┐
        │ touch /tmp/.meminsight_housekeep_1       │
        │  ↓ (signals end of data collection)      │
        │ touch /tmp/.upload_memreport             │
        │  ↓ (final upload trigger for any pending)│
        │                                          │
        │ [Wait for systemd to process triggers]   │
        └──────────────────┬───────────────────────┘
                           │
                           ▼
        ┌──────────────────────────────────────────┐
        │ Cleanup Phase:                           │
        │ • rm /tmp/.meminsight_args               │
        │ • rm /tmp/.enable_meminsight             │
        │ • rm /tmp/.meminsight_housekeep_*        │
        │ • rm /tmp/.upload_memreport              │
        │ (Keep reports in OUTPUT_DIR)             │
        └──────────────────┬───────────────────────┘
                           │
                           ▼
        ┌──────────────────────────────────────────┐
        │ EXIT FROM MEMINSIGHT BINARY              │
        │ [Return exit code to meminsight-runner]  │
        └──────────────────────────────────────────┘
```

---

## 3. UPLOAD TRIGGER CADENCE LOGIC

### Upload Decision Matrix (Inside meminsight iteration loop)

```
Per-Iteration Upload Trigger Decision:

Input Variables:
  • iteration_count (current iteration number, 0-indexed)
  • total_iterations (total from CLI arg)
  • upload_mode (from /tmp/.meminsight_args)
  • cadence_interval (derived from arguments)
  • last_upload_iteration (persistent marker)

Decision Tree:

IF (upload_mode == "NONE")
  └─► NO UPLOAD (skip)
ELSE
  IF (iteration_count == 0) AND (upload_mode == "PRE_EXEC")
    └─► TRIGGER UPLOAD (pre-execution before iteration 0 completes)
  ELSE IF (iteration_count == total_iterations-1) AND (upload_mode contains "POST")
    └─► TRIGGER UPLOAD (post-execution, last iteration done)
  ELSE IF (upload_mode contains "CADENCE")
    ├─ cadence_interval = calculated from total_iterations or explicit --upload-cadence
    ├─ iterations_since_last = iteration_count - last_upload_iteration
    ├─ IF (iterations_since_last >= cadence_interval) AND (iteration_count > 0)
    │  └─► TRIGGER UPLOAD → update last_upload_iteration
    └─ ELSE
       └─► NO UPLOAD (wait for next cadence)
ELSE
  └─► NO UPLOAD (default)

When TRIGGER UPLOAD Decision is Made:
  1. touch /tmp/.upload_memreport
  2. log: "Upload triggered at iteration $i out of $total_iterations"
  3. [systemd path unit detects change]
  4. [meminsight-upload.service is started]
  5. [upload_memReports.sh executes asynchronously]
  6. [meminsight continues next iteration without blocking]
```

### Cadence Calculation Examples

```
Example 1: Cadence per 2 iterations
  --iterations 10 --upload-cadence 2
  Uploads triggered at: iterations 2, 4, 6, 8, 9 (final)
  Files created: 5 tar uploads

Example 2: Only pre/post execution
  --iterations 10 --upload-cadence none
  Uploads triggered at: 
    • housekeep_0 → pre-exec upload
    • housekeep_1 → post-exec upload (final cleanup)
  Files created: 2 tar uploads

Example 3: Smart cadence based on total iterations
  --iterations 10 (auto cadence: 10/3 ≈ 3 iterations)
  Uploads triggered at: iterations 3, 6, 9 (final)
  Files created: 3 tar uploads
```

---

## 4. UPLOAD_MEMREPORTS.SH DETAILED FLOW

### Overall Structure

```
┌──────────────────────────────────────────────────────────────────┐
│                    UPLOAD SCRIPT ENTRY POINT                     │
│                  (fired by systemd service)                      │
└──────────────────────┬───────────────────────────────────────────┘
                       │
                       ▼
            ┌──────────────────────────┐
            │  SECTION 1: SOURCE SETUP │
            └──────────────┬───────────┘
                           │
            ┌──────────────────────────┐
            │ SECTION 2: LOCK & GUARD  │
            └──────────────┬───────────┘
                           │
            ┌──────────────────────────┐
            │ SECTION 3: PRECHECKS     │
            └──────────────┬───────────┘
                           │
            ┌──────────────────────────┐
            │ SECTION 4: DISCOVERY     │
            └──────────────┬───────────┘
                           │
            ┌──────────────────────────┐
            │ SECTION 5: PREPARATION   │
            └──────────────┬───────────┘
                           │
            ┌──────────────────────────┐
            │ SECTION 6: UPLOAD EXEC   │
            └──────────────┬───────────┘
                           │
            ┌──────────────────────────┐
            │ SECTION 7: CLEANUP       │
            └──────────────┬───────────┘
                           │
                           ▼
                      EXIT (rc)
```

### SECTION 1: Source Setup & Environment

```
Entry Point: /opt/rdk/rdk-meminsight/upload_memReports.sh

Step 1.1: Source Configuration Files
  source /etc/device.properties
  source /lib/rdk/t2Shared_api.sh
  source /lib/rdk/getpartnerid.sh
  source /lib/rdk/utils.sh
  [+ custom meminsight config if exists]

Step 1.2: Load MemInsight Runtime Context
  if [ -f /tmp/.meminsight_args ]; then
    source /tmp/.meminsight_args
  else
    # Use defaults or exit with error
    OUTPUT_DIR=${OUTPUT_DIR:-/nvram/meminsight}
    UPLOAD_MODE=${UPLOAD_MODE:-PRE_EXEC}
  fi

Step 1.3: Initialize Script Variables
  SCRIPT_NAME="upload_memReports.sh"
  SCRIPT_VERSION="1.0"
  LOG_PREFIX="[MemInsight-Upload]"
  LOCK_FILE="/run/meminsight-upload.lock"
  STAGING_DIR="/var/cache/meminsight/upload-staging"
  FAILED_QUEUE_DIR="/var/cache/meminsight/upload-failed"
  UPLOAD_LOG="/var/log/meminsight/uploads/upload.log"
  
  # Optional: Detect execution context
  if [ -f /tmp/.meminsight_housekeep_0 ]; then
    EXEC_CONTEXT="PRE_EXEC"
  elif [ -f /tmp/.meminsight_housekeep_1 ]; then
    EXEC_CONTEXT="POST_EXEC"
  else
    EXEC_CONTEXT="MID_ITERATION"
  fi

Step 1.4: Setup Logging Functions
  define: echo_t() - Timestamped output to syslog
  define: log_info() - Info level to upload log
  define: log_error() - Error level to upload log
  define: log_debug() - Debug level if DEBUG=1

  echo_t "MemInsight Upload started (context: $EXEC_CONTEXT)"
```

### SECTION 2: Lock & Guard (Singleton Prevention)

```
Step 2.1: Acquire Exclusive Lock
  if ! mkdir ${LOCK_FILE} 2>/dev/null; then
    # Lock exists, another instance running
    LOCK_PID=$(cat ${LOCK_FILE}/.pid 2>/dev/null)
    if kill -0 $LOCK_PID 2>/dev/null; then
      # Process is still alive
      echo_t "ERROR: Another upload already in progress (PID: $LOCK_PID)"
      exit 1
    else
      # Stale lock, remove it
      rm -rf ${LOCK_FILE}
      # Retry lock acquisition
      mkdir ${LOCK_FILE} || exit 1
    fi
  fi
  echo $$ > ${LOCK_FILE}/.pid

Step 2.2: Setup Cleanup Trap (Guard)
  trap 'cleanup_on_exit' EXIT INT TERM
  
  cleanup_on_exit() {
    local exit_code=$?
    echo_t "Cleanup: removing lock file"
    rm -rf ${LOCK_FILE}
    rm -f ${TAR_FILE} 2>/dev/null
    exit $exit_code
  }

Step 2.3: Guard: Ensure Required Directories Exist
  [ -d "$STAGING_DIR" ] || mkdir -p "$STAGING_DIR"
  [ -d "$FAILED_QUEUE_DIR" ] || mkdir -p "$FAILED_QUEUE_DIR"
  [ -d "$(dirname $UPLOAD_LOG)" ] || mkdir -p "$(dirname $UPLOAD_LOG)"
```

### SECTION 3: Prechecks (Validation & Prerequisites)

```
Step 3.1: Verify Network Connectivity
  Function: check_network_up()
    WAN_STATE=$(sysevent get wan-status)
    if [ "$WAN_STATE" != "started" ]; then
      echo_t "WARNING: WAN not started, deferring upload"
      return 1
    fi
    
    # Get WAN interface and verify IP
    WAN_INTERFACE=$(getWanInterfaceName)
    EROUTER_IP=$(ifconfig $WAN_INTERFACE | grep inet | awk '{print $2}')
    
    if [ -z "$EROUTER_IP" ]; then
      echo_t "ERROR: No IP on WAN interface"
      return 1
    fi
    return 0
  
  if ! check_network_up; then
    log_error "Network not available, queuing for retry"
    # Mark for retry with backoff
    echo "DEFERRED:$(date +%s)" > ${FAILED_QUEUE_DIR}/.upload_deferred
    exit 2  # Transient failure, systemd can retry
  fi

Step 3.2: Get Endpoint URL (Dynamic from DCM)
  Function: get_upload_endpoint()
    # Source from DCM configuration
    if [ -f /tmp/DCMresponse.txt ]; then
      endpoint=$(grep 'MemInsightUploadEndpoint' /tmp/DCMresponse.txt | cut -d'"' -f4)
      if [ -n "$endpoint" ]; then
        echo "$endpoint"
        return 0
      fi
    fi
    
    # Fallback to configured default
    endpoint="${UPLOAD_ENDPOINT:-https://upload-cloud.rdkcentral.com/meminsight}"
    echo "$endpoint"
    return 0
  
  UPLOAD_ENDPOINT=$(get_upload_endpoint)
  [ -z "$UPLOAD_ENDPOINT" ] && {
    log_error "Cannot determine upload endpoint"
    exit 1
  }
  log_info "Upload endpoint: $UPLOAD_ENDPOINT"

Step 3.3: Verify NTP (Time Sync)
  Function: check_ntp_sync()
    # Check if system time is reasonable (not 1970 or far future)
    CURRENT_TIME=$(date +%s)
    if [ $CURRENT_TIME -lt 1000000000 ]; then
      echo_t "WARNING: System time not synced (NTP not ready)"
      return 1
    fi
    return 0
  
  if ! check_ntp_sync; then
    log_error "System time not synchronized, deferring upload"
    exit 2  # Transient
  fi

Step 3.4: Verify Output Directory Exists
  if [ ! -d "$OUTPUT_DIR" ]; then
    log_error "Output directory not found: $OUTPUT_DIR"
    exit 1  # Permanent failure
  fi
```

### SECTION 4: Discovery (Find Reports to Upload)

```
Step 4.1: Find Reports in OUTPUT_DIR
  Function: discover_reports()
    # Pattern: MAC_TIMESTAMP_iter*_meminsight.{csv,json}
    CSV_FILES=$(find "$OUTPUT_DIR" -type f -name "*_meminsight.csv" 2>/dev/null)
    JSON_FILES=$(find "$OUTPUT_DIR" -type f -name "*_meminsight.json" 2>/dev/null)
    
    REPORT_COUNT=$(echo "$CSV_FILES $JSON_FILES" | wc -w)
    
    if [ $REPORT_COUNT -eq 0 ]; then
      log_error "No reports found in $OUTPUT_DIR"
      return 1
    fi
    
    # Return newest $BATCH_SIZE files (default 10)
    BATCH_SIZE=${BATCH_SIZE:-10}
    REPORTS=$(find "$OUTPUT_DIR" -type f -name "*_meminsight.*" \
              -newer /tmp/.meminsight_args 2>/dev/null | \
              sort -r | head -n $BATCH_SIZE)
    
    echo "$REPORTS"
    return 0
  
  REPORTS=$(discover_reports)
  if [ $? -ne 0 ]; then
    # No reports available, exit gracefully
    log_info "No reports to upload, exiting"
    exit 0  # Not an error
  fi

Step 4.2: Detect Execution Context from Flags
  Determine if this is:
  • PRE_EXEC upload (housekeep_0 exists) - before any data collected
  • MID_ITERATION upload (cadence triggered mid-run)
  • POST_EXEC upload (housekeep_1 exists) - final cleanup
  
  if [ -f /tmp/.meminsight_housekeep_1 ]; then
    CONTEXT="POST_EXEC:final_cleanup"
    # More lenient timeout, as device may be rebooting
  elif [ -f /tmp/.meminsight_housekeep_0 ]; then
    CONTEXT="PRE_EXEC:start_trigger"
    # Conservative upload (may have 0 reports)
  else
    CONTEXT="MID_ITERATION:cadence_triggered"
    # Standard upload
  fi
  
  log_info "Execution context: $CONTEXT"

Step 4.3: Count and Log Discovery
  REPORT_COUNT=$(echo "$REPORTS" | wc -l)
  TOTAL_SIZE=$(du -sh $REPORTS 2>/dev/null | tail -1 | awk '{print $1}')
  
  log_info "Discovered $REPORT_COUNT reports, total size: $TOTAL_SIZE"
  log_info "Reports: $REPORTS"  # For audit trail
```

### SECTION 5: Preparation (Tar & Compress)

```
Step 5.1: Create Staging Area
  STAGING_SUBDIR="$STAGING_DIR/upload_$(date +%s)_$$"
  mkdir -p "$STAGING_SUBDIR"
  
  # Copy reports to staging (isolate from active collection)
  for REPORT in $REPORTS; do
    cp "$REPORT" "$STAGING_SUBDIR/" 2>/dev/null || {
      log_error "Failed to copy report: $REPORT"
      continue  # Skip this file, upload what we can
    }
  done
  
  STAGED_COUNT=$(ls "$STAGING_SUBDIR" 2>/dev/null | wc -l)
  if [ $STAGED_COUNT -eq 0 ]; then
    log_error "No reports successfully staged"
    rmdir "$STAGING_SUBDIR"
    exit 1
  fi

Step 5.2: Create Tar Archive with Metadata
  TAR_TIMESTAMP=$(date +%Y%m%d_%H%M%S)
  TAR_SEQUENCER=$(($(cat $FAILED_QUEUE_DIR/.seq 2>/dev/null || echo 0) + 1))
  echo $TAR_SEQUENCER > $FAILED_QUEUE_DIR/.seq
  
  MAC=$(getMacAddressOnly)
  TAR_FILENAME="${MAC}_meminsight_${TAR_TIMESTAMP}_seq${TAR_SEQUENCER}.tar.gz"
  TAR_FILE="$STAGING_SUBDIR/$TAR_FILENAME"
  
  # Create tar with metadata file (for upload tracking)
  cat > "$STAGING_SUBDIR/METADATA.txt" << EOF
Device: $MAC
Timestamp: $TAR_TIMESTAMP
UploadContext: $CONTEXT
ReportCount: $STAGED_COUNT
UploadScript: upload_memReports.sh
ScriptVersion: 1.0
UploadedAt: $(date)
EOF

  # Compress with gzip, preserving file structure
  tar -czf "$TAR_FILE" -C "$STAGING_SUBDIR" . 2>/dev/null || {
    log_error "Failed to create tar archive"
    rm -rf "$STAGING_SUBDIR"
    exit 1
  }
  
  TAR_SIZE=$(ls -lh "$TAR_FILE" | awk '{print $5}')
  log_info "Created tar archive: $TAR_FILENAME ($TAR_SIZE)"

Step 5.3: Calculate Checksum
  TAR_MD5=$(md5sum "$TAR_FILE" | awk '{print $1}')
  TAR_BASE64_MD5=$(echo -n "$TAR_MD5" | base64)
  
  log_debug "Tar MD5: $TAR_MD5"
  log_debug "Tar MD5 (base64): $TAR_BASE64_MD5"
```

### SECTION 6: Upload Execution

```
Step 6.1: Prepare Curl Command (Following RDK Pattern)
  Leverage existing RDK functions/mechanisms:
  
  Function: prepare_curl_request()
    # Get interface with network connectivity
    WAN_INTERFACE=$(getWanInterfaceName)
    
    # Check for IPv6 support
    HAS_IPV6=$(ifconfig $WAN_INTERFACE | grep -c inet6)
    if [ $HAS_IPV6 -gt 0 ]; then
      ADDR_TYPE=""  # Let curl choose
    else
      ADDR_TYPE="-4"  # Force IPv4
    fi
    
    # TLS version (hardcoded per RDK standard)
    TLS_VERSION="--tlsv1.2"
    
    # Cert validation
    CERT_STATUS=""
    if [ -f /tmp/.EnableOCSPStapling ] || [ -f /tmp/.EnableOCSP ]; then
      CERT_STATUS="--cert-status"
    fi
    
    # Timeout settings
    CONNECT_TIMEOUT=30
    MAX_TIME=120
    
    # Build curl args
    CURL_ARGS=(
      "$TLS_VERSION"
      "$CERT_STATUS"
      "--connect-timeout $CONNECT_TIMEOUT"
      "-m $MAX_TIME"
      "-w '%{http_code}'"
      "--interface $WAN_INTERFACE"
      "$ADDR_TYPE"
      "-T $TAR_FILE"
      "-o /tmp/meminsight_upload_response.txt"
      "$UPLOAD_ENDPOINT"
    )
    
    echo "${CURL_ARGS[@]}"
  
  CURL_ARGS=$(prepare_curl_request)

Step 6.2: Attempt Upload with Retry Loop (RDK Pattern)
  MAX_RETRIES=3
  RETRY_COUNT=0
  UPLOAD_SUCCESS=0
  
  Function: execute_upload()
    local attempt=$1
    
    log_info "Upload attempt $attempt/$MAX_RETRIES"
    
    # Execute curl
    HTTP_CODE=$(curl ${CURL_ARGS[@]} 2>&1 | tail -1)
    CURL_EXIT=$?
    
    log_debug "curl exit code: $CURL_EXIT, HTTP code: $HTTP_CODE"
    
    # Evaluate result
    case $HTTP_CODE in
      200)
        log_info "Upload SUCCESS (HTTP 200)"
        return 0
        ;;
      201|204)
        log_info "Upload SUCCESS (HTTP $HTTP_CODE)"
        return 0
        ;;
      302)
        log_error "Redirect (HTTP 302) - endpoint may have moved"
        return 2  # Transient
        ;;
      400|401|403)
        log_error "Client error (HTTP $HTTP_CODE) - credentials or auth issue"
        return 1  # Permanent failure
        ;;
      500|502|503|504)
        log_error "Server error (HTTP $HTTP_CODE) - backend issue"
        return 2  # Transient, retry
        ;;
      *)
        log_error "Unexpected HTTP code: $HTTP_CODE"
        return 2  # Assume transient
        ;;
    esac
  }
  
  # Retry loop with backoff (RDK pattern)
  while [ $RETRY_COUNT -lt $MAX_RETRIES ]; do
    execute_upload $((RETRY_COUNT + 1))
    UPLOAD_RC=$?
    
    if [ $UPLOAD_RC -eq 0 ]; then
      UPLOAD_SUCCESS=1
      break
    elif [ $UPLOAD_RC -eq 1 ]; then
      # Permanent failure, don't retry
      break
    fi
    
    # Transient failure, apply backoff
    RETRY_COUNT=$((RETRY_COUNT + 1))
    if [ $RETRY_COUNT -lt $MAX_RETRIES ]; then
      BACKOFF_TIME=$((20 * RETRY_COUNT))  # 20, 40, 60, ...
      log_info "Backoff for $BACKOFF_TIME seconds before retry"
      sleep $BACKOFF_TIME
    fi
  done

Step 6.3: Handle Upload Result
  if [ $UPLOAD_SUCCESS -eq 1 ]; then
    log_info "Upload completed successfully"
    UPLOAD_RESULT="SUCCESS"
  else
    log_error "Upload failed after $MAX_RETRIES attempts"
    UPLOAD_RESULT="FAILED"
    
    # Queue for retry (exponential backoff)
    FAILED_QUEUE_ENTRY="$FAILED_QUEUE_DIR/$(basename $TAR_FILE)"
    mv "$TAR_FILE" "$FAILED_QUEUE_ENTRY" || {
      log_error "Failed to queue for retry"
    }
    log_info "Queued for retry: $(basename $TAR_FILE)"
  fi
```

### SECTION 7: Cleanup & Finalization

```
Step 7.1: Log Upload Transaction
  cat >> "$UPLOAD_LOG" << EOF
$(date): Upload=$UPLOAD_RESULT, File=$(basename $TAR_FILE), Context=$CONTEXT, Size=$TAR_SIZE, MD5=$TAR_MD5, HTTPCode=$HTTP_CODE
EOF

Step 7.2: Post-Upload Report Cleanup
  IF (UPLOAD_SUCCESS == 1):
    # Remove successfully uploaded reports from OUTPUT_DIR
    for REPORT in $REPORTS; do
      rm -f "$REPORT" 2>/dev/null || {
        log_error "Failed to remove: $REPORT (may be in use)"
      }
    done
    
    # Remove staging directory
    rm -rf "$STAGING_SUBDIR"
    log_info "Cleaned up staged reports"
  ELSE:
    # Keep staged reports for retry
    # Keep tar in failed queue
    log_info "Preserved failed upload for retry"

Step 7.3: Enforce Retention Policy
  Function: enforce_retention()
    # Max-size policy: If total uploads > RETENTION_MAX_SIZE, remove oldest
    TOTAL_SIZE=$(du -sh "$FAILED_QUEUE_DIR" 2>/dev/null | awk '{print $1}' | sed 's/G/*1024M/;s/M.*//' | bc)
    MAX_SIZE_MB=${RETENTION_MAX_SIZE:-500}
    
    if [ $TOTAL_SIZE -gt $MAX_SIZE_MB ]; then
      log_info "Retention: removing old failed uploads (current: ${TOTAL_SIZE}MB, max: ${MAX_SIZE_MB}MB)"
      find "$FAILED_QUEUE_DIR" -type f -name "*.tar.gz" | sort | head -1 | xargs rm -f
    fi
    
    # Max-age policy: Remove failed uploads older than 24 hours
    MAX_AGE_HOURS=${RETENTION_MAX_AGE:-24}
    find "$FAILED_QUEUE_DIR" -type f -name "*.tar.gz" -mtime +$MAX_AGE_HOURS -delete
    
    # Max-files policy: Keep only last N failed uploads
    MAX_FILES=${RETENTION_MAX_FILES:-20}
    FAILED_COUNT=$(ls "$FAILED_QUEUE_DIR"/*.tar.gz 2>/dev/null | wc -l)
    if [ $FAILED_COUNT -gt $MAX_FILES ]; then
      ls -t "$FAILED_QUEUE_DIR"/*.tar.gz | tail -n +$((MAX_FILES+1)) | xargs rm -f
    fi
  
  enforce_retention

Step 7.4: Final Logging & Exit
  if [ $UPLOAD_SUCCESS -eq 1 ]; then
    log_info "Upload cycle completed successfully"
    t2CountNotify "MEMINSIGHT_UPLOAD_SUCCESS"  # RDK telemetry marker
    exit 0
  else
    log_error "Upload cycle failed, queued for retry"
    t2CountNotify "MEMINSIGHT_UPLOAD_FAILED"   # RDK telemetry marker
    exit 2  # Transient failure (systemd can retry)
  fi
```

---

## 5. SYSTEMD UNIT FILES

### meminsight-runner.path

```ini
[Path]
PathExists=/tmp/.enable_meminsight
Unit=meminsight-runner.service

[Install]
WantedBy=multi-user.target
```

### meminsight-runner.service

```ini
[Unit]
Description=MemInsight Memory Analysis Runner
After=network-online.target
Requires=network-online.service
Documentation=man:meminsight(8)

[Service]
Type=oneshot
ExecStart=/usr/bin/meminsight --iterations 10 --interval 300
StandardOutput=journal
StandardError=journal
SyslogIdentifier=meminsight

# Resource limits
MemoryLimit=256M
CPUQuota=30%

# Restart policy
RemainAfterExit=no

[Install]
WantedBy=multi-user.target
```

### meminsight-upload.path

```ini
[Path]
PathModified=/tmp/.upload_memreport
Unit=meminsight-upload.service

# Don't process path events too rapidly
DirectoryNotEmpty=/var/cache/meminsight/upload-staging

[Install]
WantedBy=multi-user.target
```

### meminsight-upload.service

```ini
[Unit]
Description=MemInsight Report Upload Service
After=network-online.target meminsight-runner.service
Requires=network-online.service
Documentation=man:upload_memReports(1)

[Service]
Type=oneshot
ExecStart=/opt/rdk/rdk-meminsight/upload_memReports.sh
StandardOutput=journal
StandardError=journal
SyslogIdentifier=meminsight-upload

# Execution context
Environment="DEBUG=0"
Environment="BATCH_SIZE=10"
Environment="RETENTION_MAX_SIZE=500"
Environment="RETENTION_MAX_AGE=24"

# Resource limits (uploads are I/O intensive)
MemoryLimit=128M
CPUQuota=20%

# Timeout: allow up to 5 minutes for upload
TimeoutStartSec=300

# If upload fails, systemd won't restart (oneshot)
# Manual retry will be triggered by next path event
RemainAfterExit=no

[Install]
WantedBy=multi-user.target
```

---

## 6. ARGUMENT & STATE MANAGEMENT

### /tmp/.meminsight_args Format

```ini
# Basic execution parameters
ITERATIONS=10
INTERVAL=300
OUTPUT_DIR=/nvram/meminsight
OUTPUT_FORMAT=csv,json
ENABLE_COMPRESSION=1

# Upload trigger mode
UPLOAD_MODE=cadence
UPLOAD_CADENCE=3              # Upload every 3 iterations
UPLOAD_ON_COMPLETION=1        # Also upload at end

# Fragmentation source (cached at startup per earlier work)
FRAGMENTATION_SOURCE=auto    # auto, pagetypeinfo, buddyinfo, none

# State markers
START_TIME=1681234567
PID=$$
HOSTNAME=$(hostname)
KERNEL_VERSION=$(uname -r)

# Execution context (set by meminsight)
EXEC_MODE=SERVICE             # SERVICE or CLI
TRIGGERED_BY=RFC              # RFC or MANUAL
```

### /tmp/.meminsight_housekeep_* Files

```
• Created by: meminsight.c
• Purpose: Signal housekeeping boundary to upload script

/tmp/.meminsight_housekeep_0:
  - Created before any data collection begins
  - File content: Upload context identifier
  - Upload triggered: Check for initial reports (may be zero)

/tmp/.meminsight_housekeep_1:
  - Created after all iterations complete
  - File content: Final context identifier
  - Upload triggered: Force final upload of pending reports
```

### /tmp/.upload_memreport Signal File

```
• Touch triggers: systemd path unit → meminsight-upload.service
• Created by: meminsight.c during iterations or housekeeping
• Monitored by: meminsight-upload.path (PathModified=/tmp/.upload_memreport)
• Pattern: 
    touch /tmp/.upload_memreport   (creates file)
    systemd detects change
    START meminsight-upload.service
    upload_memReports.sh executes
    May delete or modify this file after processing
```

---

## 7. ERROR HANDLING & SIGNAL FLOW

### Permanent vs. Transient Failures

```
PERMANENT FAILURES (exit 1): Don't retry
  • Output directory doesn't exist
  • Invalid arguments to meminsight
  • Authentication failure (HTTP 401, 403)
  • Corrupt/invalid report files
  • Disk quota exceeded

TRANSIENT FAILURES (exit 2): systemd will retry
  • Network temporarily down (WAN state = stopped)
  • NTP not synced
  • Upstream server error (HTTP 5xx)
  • Timeout on curl
  • Staging dir creation failed

GRACEFUL NO-OP (exit 0): Not an error
  • No reports found for upload
  • Already uploading (lock present and process alive)
  • Upload dir empty or doesn't exist yet
```

### Systemd Error Recovery

```
When upload_memReports.sh exits with code 2 (transient):
  • systemd-path continues monitoring
  • Next /tmp/.upload_memreport modification triggers retry
  • Exponential backoff within script handles immediate retries
  • If failed tar exists in queue, next execution processes retry queue

When upload_memReports.sh exits with code 1 (permanent):
  • Logged as alert
  • Failed tar archived for manual investigation
  • Next upload cycle skips permanently-failed tars
  • Administrator must intervene to retry

When upload_memReports.sh exits with code 0 (success):
  • Reports deleted from OUTPUT_DIR
  • Staging cleaned up
  • Audit logged
```

---

## 8. INTEGRATION POINTS WITH EXISTING RDK SCRIPTS

### Pattern: Properties & Device Info

| RDK Function/File | MemInsight Usage |
|---|---|
| `/etc/device.properties` | Source to get BOX_TYPE, partner ID |
| `/lib/rdk/t2Shared_api.sh` | Telemetry markers (t2CountNotify) |
| `/lib/rdk/getpartnerid.sh` | For MAC identification in tar names |
| `/lib/rdk/utils.sh` | Logging utilities (echo_t) |
| `getMacAddressOnly()` | Compute unique device identifier |
| `getWanInterfaceName()` | Determine upload interface |
| `getIPAddress()` | Verify network connectivity |
| `getPartnerId()` |Partner identification for DCM URL |

### Pattern: Upload Endpoint Discovery

Leverage RDK's DCM (Device Config Manager) patterns:
```bash
# In upload_memReports.sh
if [ -f /tmp/DCMresponse.txt ]; then
  ENDPOINT=$(grep 'MemInsightUploadEndpoint:URL' /tmp/DCMresponse.txt | cut -d'"' -f4)
fi

# If not in DCM, fallback to hardcoded (per RDK pattern)
ENDPOINT="${ENDPOINT:-https://upload.rdkcentral.com/meminsight}"
```

### Pattern: Event Signaling

Use RDK's sysevent framework:
```bash
sysevent set meminsight_upload_status "running"
sysevent set meminsight_upload_status "complete:success"
sysevent set meminsight_upload_status "complete:failed"
```

### Pattern: Retry & Backoff Strategy

Following RDK's random_sleep + exponential backoff:
```bash
random_sleep() {
  local attempt=$1
  local t_min=$((20 * attempt))
  local t_max=$((30 * attempt))
  local randomizedNumber=$((RANDOM % (t_max - t_min + 1) + t_min))
  sleep $randomizedNumber
}
```

---

## 9. SEQUENCE DIAGRAM: Complete Flow

```
RFC Enable → Meminsight Binary → Housekeeping → Upload Flow → Cleanup

Time  │
      │  ┌─── RFC Set by DCM
      ▼  │   (Device.DeviceInfo...MemInsight.Enable=true)
         │
      0ms├─► Touch /tmp/.enable_meminsight
         │   └──► systemd-path detects change
         │
     50ms├─► START meminsight-runner.service
         │
    100ms├─► meminsight binary starts
         │   ├─ Parse arguments
         │   ├─ Dump to /tmp/.meminsight_args
         │   └─ log: "Execution started"
         │
    150ms├─► Housekeep Phase 0
         │   ├─ touch /tmp/.meminsight_housekeep_0
         │   ├─ touch /tmp/.upload_memreport
         │   └─► systemd-path detects → START meminsight-upload.service
         │
    200ms├─► upload_memReports.sh starts (CONTEXT=PRE_EXEC)
         │   ├─ Source configs
         │   ├─ Acquire lock
         │   ├─ Check network
         │   ├─ Discover reports (likely 0 at this point)
         │   └─ Exit gracefully (exit 0: no reports)
         │
    300ms├─► Iteration Loop Starts [i=0]
         │   ├─ Collect meminfo, smaps, pagetypeinfo, ...
         │   ├─ Write reports (iteration_0_meminsight.csv/json)
         │   └─ Check cadence (no upload trigger yet)
         │
    800ms├─► Iteration 1 completes
         │   ├─ Write reports (iteration_1_meminsight.csv/json)
         │   ├─ Check cadence: if (upload cadence == 2) → TRIGGER
         │   ├─ touch /tmp/.upload_memreport
         │   └─► systemd-path detects → START meminsight-upload.service
         │
    900ms├─► upload_memReports.sh starts (CONTEXT=MID_ITERATION)
         │   ├─ Acquire lock
         │   ├─ Check network
         │   ├─ Discover reports (iteration_0, iteration_1 found)
         │   ├─ Tar and compress
         │   ├─ Curl to endpoint → HTTP 200 SUCCESS
         │   ├─ Delete source reports
         │   └─ Exit (exit 0)
         │
   4500ms├─► Iterations complete [i=9]
         │   ├─ Write final reports
         │   ├─ Check cadence (last iteration, trigger final)
         │   │
   4600ms├─► Housekeep Phase 1 (END)
         │   ├─ touch /tmp/.meminsight_housekeep_1
         │   ├─ touch /tmp/.upload_memreport
         │   └─► systemd-path detects → START meminsight-upload.service
         │
   4700ms├─► upload_memReports.sh starts (CONTEXT=POST_EXEC)
         │   ├─ Acquire lock
         │   ├─ Check network
         │   ├─ Discover reports (remaining undelivered)
         │   ├─ Tar and compress
         │   ├─ Curl to endpoint → HTTP 200 SUCCESS
         │   ├─ Delete source reports
         │   ├─ Cleanup staging & queue
         │   └─ Exit (exit 0)
         │
   4800ms├─► Cleanup Phase (Meminsight.c)
         │   ├─ rm /tmp/.meminsight_args
         │   ├─ rm /tmp/.enable_meminsight
         │   ├─ rm /tmp/.meminsight_housekeep_*
         │   ├─ rm /tmp/.upload_memreport
         │   └─ EXIT (exit 0)
         │
   4850ms└─► meminsight-runner.service completes
                └──► All reports uploaded, flow complete ✓
```

---

## 10. KEY DESIGN DECISIONS & RATIONALE

| Decision | Rationale |
|---|---|
| **Separate upload script** | Decouples collection from upload; upload failures don't block collection |
| **systemd path trigger** | Event-driven, no polling; integrates with RDK standard infrastructure |
| **File-based signaling** | Simple, reliable, no IPC complexity; works across process boundaries |
| **Lock file in /run** | Prevents concurrent uploads; /run is ephemeral, auto-cleanup on reboot |
| **Cadence-based bucketing** | Reduces upload frequency; batches reports for efficiency |
| **Retry queue in /var/cache** | Survives temporary network issues; persists across iterations |
| **Tar + gzip** | Reduces upload bandwidth; single file per batch for atomic ops |
| **Metadata in tar** | Audit trail; helps troubleshoot upload issues |
| **MD5 checksum** | Supports optional encryption/verification patterns |
| **Staging directory** | Isolates from active reports; allows concurrent write during upload |
| **Retention policy** | Prevents disk exhaustion; configurable per deployment |
| **Backoff + random jitter** | Prevents thundering herd; follows RDK pattern |

---

## 11. CONFIGURATION REFERENCE

### Environment Variables (upload_memReports.sh)

```bash
# Directories
OUTPUT_DIR         Default: /nvram/meminsight
STAGING_DIR        Default: /var/cache/meminsight/upload-staging
FAILED_QUEUE_DIR   Default: /var/cache/meminsight/upload-failed
UPLOAD_LOG         Default: /var/log/meminsight/uploads/upload.log

# Behavior
BATCH_SIZE         Default: 10 (reports per upload)
MAX_RETRIES        Default: 3 (retry attempts per upload)
MAX_TIME           Default: 120 (seconds, curl timeout)
CONNECT_TIMEOUT    Default: 30 (seconds)

# Retention
RETENTION_MAX_SIZE Default: 500 (MB, failed queue max size)
RETENTION_MAX_AGE  Default: 24 (hours, remove old failures)
RETENTION_MAX_FILES Default: 20 (max failed tars in queue)

# Debug
DEBUG              Default: 0 (set to 1 for verbose logging)
DRY_RUN            Default: 0 (set to 1 to test without curl)
```

### Meminsight CLI Arguments (New)

```bash
--iterations N              Total iterations to run
--interval S               Sleep between iterations (seconds)
--output-dir /path         Where to write reports (default: /nvram/meminsight)
--output-format csv,json   Comma-separated list of formats
--upload-mode cadence      none|pre|post|cadence|pre+post|all
--upload-cadence N         Upload every N iterations (implies --upload-mode cadence)
--upload-on-completion     Upload final batch after all iterations
--retention-max-size MB    Max disk for failed uploads
--retention-max-age HOURS  Remove failed uploads older than N hours
--enable-compression 0|1   Gzip the tar (default: 1)
```

---

## 12. TESTING SCENARIOS

| Scenario | Test Case |
|---|---|
| **Happy Path** | RFC enabled → 10 iterations with cadence 3 → 4 uploads → all success |
| **Network Down Start** | WAN down at start → upload deferred → WAN comes up → upload proceeds |
| **Network Down Mid** | Upload during iteration → curl timeout → retry → succeed |
| **No Reports** | PRE_EXEC context → no reports found → exit gracefully |
| **Concurrent Upload** | Two path triggers within 100ms → lock prevents concurrent execution |
| **Stale Lock** | Lock file exists but PID not running → lock removed → new instance acquires |
| **Tar Creation Fails** | Disk full → tar fails → queued for retry → next cycle retries |
| **Retention Policy** | 100 failed tars (max=20) → oldest 80 deleted → only 20 remain |
| **Device Reboot During** | Upload in progress → lock file removed on shutdown → next boot retries |
| **Custom Endpoint** | DCM provides custom endpoint → used instead of default |
| **IPv6 Network** | Only IPv6 available → curl forced to IPv6 mode → succeed |
| **Corrupted Report** | One CSV corrupt → skip it → tar remaining reports → upload rest |

---

## 13. NEXT STEPS FOR REFINEMENT

1. **Confirm cadence logic**: Is "every N iterations" or "every N seconds of wall-clock time"?
2. **Endpoint specification**: Exact URL format and parameter passing to curl?
3. **Encryption handling**: Should tars be encrypted before upload? Who manages keys?
4. **Telemetry integration**: Exact t2CountNotify marker names and format?
5. **Systemd path conditions**: PathModified vs PathExists vs DirectoryNotEmpty behavior?
6. **Pre-release/beta uploads**: Separate queue or same as production?
7. **Agent/customer comms**: Email/SMS on failed uploads, or silent retry forever?
8. **Audit trail detail**: Log every curl attempt, or summary only?
9. **Docker/container scenario**: Paths differ in containerized deployments?
10. **Partner-specific overrides**: How to allow custom upload script per partner?

---
