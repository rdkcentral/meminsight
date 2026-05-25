/*
 * Copyright 2024 Comcast Cable Communications Management, LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef MEMINSIGHT_H
#define MEMINSIGHT_H

// -----------------------------
// Standard C includes
// -----------------------------

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/sched.h> // For PF_KTHREAD (Linux only)
#include <net/if.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>


// -----------------------------
// Debug Macros
// -----------------------------

#define PRINT_MUST printf

#ifdef TESTME
#define PRINT_ERROR printf
#else
#define PRINT_ERROR(...)
#endif

#if 0
#define PRINT_INFO printf
#define PRINT_DBG_INITIAL printf
#define PRINT_DBG_SCANNED printf
#define PRINT_DBG printf
#else
#define PRINT_INFO(...)
#define PRINT_DBG_INITIAL(...)
#define PRINT_DBG_SCANNED(...)
#define PRINT_DBG(...)
#endif

// -----------------------------
// Macro Definitions
// -----------------------------

#define MEMINSIGHT_BIN "meminsight"
/* MEMINSIGHT_MAJOR_VERSION, MEMINSIGHT_MINOR_VERSION, and MEMINSIGHT_PATCH_VERSION are to track the binary version, any changes in the binary version should increment this version.
This is used to ensure compatibility with older versions of the binary. */
#define MEMINSIGHT_MAJOR_VERSION "1"
#define MEMINSIGHT_MINOR_VERSION "1"
#define MEMINSIGHT_PATCH_VERSION "1"

/* REPORT_MAJOR_VERSION, REPORT_MINOR_VERSION, and REPORT_PATCH_VERSION are to track the report format, any changes in the report format should increment this version.
This is used to ensure compatibility with older versions of the report parser. */
#define REPORT_MAJOR_VERSION "1"
#define REPORT_MINOR_VERSION "1"
#define REPORT_PATCH_VERSION "0"

#ifndef DEVICE_INTERFACE_KEY
#define DEVICE_INTERFACE_KEY "ESTB_INTERFACE"
#endif

/* Paths */
#define PROC_DIR "/proc"
#define VERSION_FILE "/version.txt"
#define DEVICE_PROP_FILE "/etc/device.properties"
#define MEMINFO_FILE PROC_DIR "/meminfo"
#define UPTIME_FILE PROC_DIR "/uptime"
#define BW_DDR_MODE_FILE "/sys/class/aml_ddr/mode"
#ifndef BW_DDR_FILE
#define BW_DDR_FILE "/sys/class/aml_ddr/bandwidth"
#endif
#ifndef BUDDYINFO_FILE
#define BUDDYINFO_FILE PROC_DIR "/buddyinfo"
#endif
#ifndef PGT_FILE
#define PGT_FILE PROC_DIR "/pagetypeinfo"
#endif

#define MEMINSIGHT_CONFIGSTORE_PATH "/tmp/.meminsight_configstore"
#define MEMINSIGHT_UPLOAD_MARKER_PATH "/tmp/.meminsight_upload"
#define MEMINSIGHT_INPROGRESS_FILE "/tmp/.meminsight_inprogress"

/* Default Macros */
#define DEFAULT_FW_NAME "ACTIVEFW123"
#define FW_LEN 64
#define KERNEL_LEN 64
#define MAC_LEN 32
#define DEFAULT_ITERATIONS 1
#define DEFAULT_INTERVAL 5
#define DEFAULT_LOG_LEVEL "INFO"
#define DEFAULT_MAC "000000000000"

#ifndef DEFAULT_OUT_DIR
#define DEFAULT_OUT_DIR "/opt/meminsight"
#endif

#define PF_KTHREAD 0x00200000               // Kernel thread flag

/* Config Macros */
#define CONFIG_EXTN ".conf"
#define CSV_FILE_NAME "meminsight.csv"
#define JSON_FILE_NAME "meminsight.json"
#define LONG_RUN_INTERVAL 900 // 900 is Default interval for long runs in seconds
#define LONG_RUN_ITERATIONS 48 // 12-hour capture at 15-minute interval; caller may override via CLI/config

#define CSV_META_HEADER "FIRMWARE_NAME,MAC_ADDRESS,TIMESTAMP,UPTIME,KERNEL_VERSION,REPORT_VERSION,ITERATION,RUN_ITERATIONS,RUN_INTERVAL,RUN_ID"

#define CSV_PROCESSES_SECTION_HEADER "\nProcesses:\n"
#define CSV_PROCESS_HEADER "PID,EXE,RSS,PSS,SHARED_CLEAN,PRIVATE_CLEAN,PRIVATE_DIRTY,SWAP_PSS,MIN_FAULTS,MAJ_FAULTS,CPU_TIME"

#define CSV_FRAGMENTATION_SECTION_HEADER "\nFragmentation"
#define CSV_STAT_VALUE_HEADER "STAT,VALUE"

#define CSV_BANDWIDTH_HEADER "TotalBandwidth,UsagePercentage"
#define CSV_BANDWIDTH_SECTION_HEADER "\nBandwidth:\n"

// -----------------------------
// Data Structures
// -----------------------------

/**
 * Struct to hold per-process memory and CPU statistics.
 * Linked list node for sorting and output.
 */
typedef struct process_info
{
    unsigned pid;                      // Process ID
    char name[PATH_MAX + 8];           // Process name
    unsigned long rssTotal;            // Resident Set Size total (kB)
    unsigned long pssTotal;            // Proportional Set Size total (kB)
    unsigned long shared_clean_total;  // Shared clean memory (kB)
    unsigned long private_clean_total;  // Private clean memory (kB)
    unsigned long private_dirty_total; // Private dirty memory (kB)
    unsigned long swap_pss_total;      // Swap PSS (kB)
    unsigned long minFaults;           // Minor page faults
    unsigned long majFaults;           // Major page faults
    unsigned long cputime;             // CPU time (user + system)
    struct process_info *next;         // Next node in linked list
} Process_Info;

/**
 * Struct to hold configuration data parsed from config file.
 */
typedef struct config
{
    char **whitelist;            // Array of whitelisted process names
    unsigned int whiteListCount; // Number of whitelisted processes
    char outputDir[PATH_MAX];    // Output directory
    unsigned int iterations;     // Number of iterations to run
    unsigned int interval;       // Interval between iterations in seconds
    char logLevel[8];            // Log level (e.g., "DEBUG", "INFO", "ERROR")
} Config_Data;

/*
 * Struct to hold setup information for report generation (one-time cache)
 * kernelVersion is cached once at startup and reused across all iterations.
 * uptime and timestamp are calculated fresh on each report iteration.
 */
typedef struct {
    char mac[MAC_LEN];
    char fwName[FW_LEN];
    char kernelVersion[KERNEL_LEN];
    char runHash[32];
    const char *outputDir;
    const char *reportFileName;
    bool dirCreated;
} SetupInfo;

/*
 * Report output format
 */
typedef enum
{
    REPORT_CSV,
    REPORT_JSON
} Report_Format;

// -----------------------------
// Global Variables
// -----------------------------
extern Process_Info getProcessInfo;   // Temporary struct for collecting process info
extern Process_Info *headProcessInfo; // Head of linked list
extern bool g_bwDataAvailable;
extern bool g_CollectFragData;        // Collect fragmentation data only when --frag is passed
extern Report_Format g_reportFormat;  // Active output format (default: REPORT_CSV)
extern bool g_jsonPrettyPrint;        // Pretty-print JSON when true

#ifdef TESTME
extern unsigned isTestMode;
extern char testSmap[128];
extern char testMeminfo[128];
extern char testBuddyinfo[128];
extern char testPagetypeinfo[128];
extern Process_Info processInfoTest;
void checkAndFree();
void testList();
#endif

// -----------------------------
// Function Prototypes
// -----------------------------
SetupInfo initializeSetupInfo(const char *outDir, Report_Format format);

void writeProcessInfo(unsigned noOfpids, FILE *output);
void addProcessInfo(Process_Info *addPInfo);
int getProcessInfos(unsigned pid);
void printHelp(char *argv[]);
void printHelpAndUsage(char *argv[], bool moreInfo, int returnCode);
void saveMeminfo(FILE *out);
void saveFragmentationInfo(FILE *out);
void collectBandwidthData(FILE *out);
int getDeviceProperty(const char *key, char *value, size_t valueLen);
size_t getMacAddress(const char *iface, char *macAddress, size_t szBufSize);
int getFirmwareImageName(char *fwName, size_t fwNameLen);
const char *getSystemUptime(void);
const char *getKernelVersion(void);

int isPID(const char *str);
int getPIDByProcessName(const char *procName, unsigned int *pidOut);
int parseConfig(const char *configFile, Config_Data *config);
int collectSystemMemoryStats(bool enableKThreads, const char *outDir, int iterations, int interval, bool long_run, bool upload_enabled, int upload_interval);
int handleConfigMode(const char *confFile, const char *cli_out_dir, bool cli_output_set, int cli_iterations, int cli_interval, bool enableKThreads, bool long_run, bool upload_enabled, int upload_interval);
int fillProcessStatFields(unsigned pid, Process_Info *info, unsigned *flagsOut);

#ifdef ENABLE_CJSON
/* Opaque cJSON type loaded via dlopen; no dependency on cJSON headers. */
typedef struct cJSON cJSON_t;

/*
 * JSON output functions - cJSON is loaded at runtime via dlopen.
 * No cjson/cJSON.h include is needed at build time.
 */
void saveMeminfo_JSON(cJSON_t *root);
void saveFragmentationInfo_JSON(cJSON_t *root);
void writeProcessInfo_JSON(cJSON_t *processesArray);
int writeJSONToFile(const char *filepath, const SetupInfo *setup);
#endif

#endif // MEMINSIGHT_H
