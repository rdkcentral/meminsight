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

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
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
void printHelp(int argc, char *argv[]);
void saveMeminfo(FILE *out);

#endif // MEMSTATUS_H
