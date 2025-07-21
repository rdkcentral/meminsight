/*
 * memstatus.h
 *
 * Header file for memstatus.c: collects and reports memory usage statistics for all processes on a Linux system.
 *
 * Contains:
 *  - Standard and Linux-specific includes
 *  - Debug macros
 *  - Process_Info struct definition
 *  - Global variable extern declarations
 *  - Function prototypes
 *
 * Usage:
 *   Include this header in memstatus.c and any other source files that need access to process info structures, macros, or function declarations.
 *
 * Author: Jagadheesan Duraisamy
 * Date: 09/07/2025
 */

#ifndef MEMSTATUS_H
#define MEMSTATUS_H

// -----------------------------
// Standard C includes
// -----------------------------

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <sys/socket.h>
#include <linux/sched.h> // For PF_KTHREAD (Linux only)

#define TESTME

// -----------------------------
// Debug Macros
// -----------------------------

//#define PRINT_DBG printf
#define PRINT_DBG(...)
#define PRINT_DBG_INITIAL printf
#define PRINT_DBG_SCANNED printf
//#define PRINT_DBG_SCANNED(...)

// -----------------------------
// Macro Definitions
// -----------------------------

#define XMEM_BIN "xMemInsight"
#define PROC_DIR "/proc"
#define DEVICE_PROP_FILE "/etc/device.properties"
#define INTERFACE "eth0"
#define DEFAULT_ITERATIONS 1
#define DEFAULT_INTERVAL 0
#define DEFAULT_OUT_DIR "/tmp"
#define DEFAULT_LOG_LEVEL "INFO"
#define DEFAULT_MAC "00:00:00:00:00:00"
#define PF_KTHREAD 0x00200000 // Kernel thread flag
#define STAT_PATH_FMT PROC_DIR "/%u/stat" // Format for process stat file path
#define SMAPS_PATH_FMT PROC_DIR "/%u/smaps" // Format for process smaps file path
#define CONFIG_EXTN ".conf"
#define CSV_FILE_NAME "meminsight.csv"

// -----------------------------
// Data Structures
// -----------------------------

/**
 * Struct to hold per-process memory and CPU statistics.
 * Linked list node for sorting and output.
 */
typedef struct process_info {
	unsigned pid;                        // Process ID
	char name[PATH_MAX+8];               // Process name
	unsigned long rssTotal;              // Resident Set Size total (kB)
	unsigned long pssTotal;              // Proportional Set Size total (kB)
	unsigned long shared_clean_total;    // Shared clean memory (kB)
	unsigned long private_dirty_total;   // Private dirty memory (kB)
	unsigned long swap_pss_total;        // Swap PSS (kB)
	unsigned long majFaults;             // Major page faults
	unsigned long cputime;               // CPU time (user + system)
	struct process_info *next;           // Next node in linked list
} Process_Info;

/**
 * Struct to hold configuration data parsed from config file.
 */
typedef struct config {
	char **whitelist;                   // Array of whitelisted process names
	unsigned int whiteListCount;        // Number of whitelisted processes
	//const char *outputFile;           // Output file name
	const char outputDir[PATH_MAX];     // Output directory
	unsigned int iterations;            // Number of iterations to run
	unsigned int interval;              // Interval between iterations in seconds
	char logLevel[8];                   // Log level (e.g., "DEBUG", "INFO", "ERROR")
} Config_Data;

// -----------------------------
// Global Variables
// -----------------------------

extern int includeKthreads;           // Whether to include kernel threads
extern Process_Info getProcessInfo; // Temporary struct for collecting process info
extern Process_Info *headProcessInfo; // Head of linked list

#ifdef TESTME
extern int testpid; // Used for test mode with custom smap file
extern char testSmap[128];
extern Process_Info processInfoTest;
void checkAndFree();
void testList();
#endif

// -----------------------------
// Function Prototypes
// -----------------------------

void writeProcessInfo(unsigned noOfpids, FILE *output);
void addProcessInfo(Process_Info *addPInfo);
int getProcessInfos(unsigned pid);
void printHelp(char *argv[]);
void printHelpAndUsage(char *argv[], bool moreInfo);
void saveMeminfo(FILE *out);
int getPropertyFromFile(const char *filename, const char *property, char *propertyValue, size_t propertyValueLen);
size_t getMacAddress(const char* iface, char *macAddress, size_t szBufSize);
int isPID(const char *str);
int getPIDByProcessName(const char *procName, unsigned int *pidOut);
int parseConfig(const char *configFile, Config_Data *config);
int collectSystemMemoryStats(bool includeKthreads, const char *outDir, int iterations, int interval);
int fillProcessStatFields(unsigned pid, Process_Info *info, unsigned *flagsOut);

#endif // MEMSTATUS_H
