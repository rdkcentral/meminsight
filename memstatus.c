/*
 * memstatus.c
 *
 * Collects and reports memory usage statistics for all processes on a Linux system.
 *
 * Features:
 *  - Scans /proc for all processes, parses /proc/[pid]/stat and /proc/[pid]/smaps
 *  - Collects RSS, PSS, shared clean, private dirty, swap PSS, major faults, and CPU time
 *  - Outputs a CSV file with per-process and system memory info
 *  - Supports test mode for validating parsing logic
 *  - Optionally includes kernel threads
 *
 * Usage:
 *   ./memstatus [-a] [-o <output directory>] [-t <path to custom smap>]
 *
 * Author: Jagadheesan Duraisamy
 * Date: 09/07/2025
 */

#include "memstatus.h"

int includeKthreads = 0;           // Whether to include kernel threads
Process_Info getProcessInfo = {0}; // Temporary struct for collecting process info
Process_Info *headProcessInfo = NULL; // Head of linked list

#ifdef TESTME
int testpid;
char testSmap[128];
Process_Info processInfoTest;
#endif
// -----------------------------
// Linked List Operations
// -----------------------------

/**
 * Writes all process info from the linked list to stdout and the output file.
 * Frees each node after writing.
 */
void writeProcessInfo(unsigned noOfpids, FILE *output)
{
    Process_Info *tmp = headProcessInfo;
    Process_Info *tofree;
    int i = 1;
    while (tmp) {
        printf("%d,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%u,%s\n",
                i++, tmp->pssTotal, tmp->rssTotal, tmp->shared_clean_total, tmp->private_dirty_total, tmp->swap_pss_total, 
                tmp->majFaults, tmp->cputime, tmp->pid, tmp->name);
        fprintf(output, "%u,%s,%lu,%lu,%lu,%lu,%lu,%lu,%lu\n", tmp->pid, tmp->name, tmp->pssTotal, tmp->rssTotal, tmp->shared_clean_total,
                tmp->private_dirty_total, tmp->swap_pss_total, tmp->majFaults, tmp->cputime);    
        tofree = tmp;
        tmp = tmp->next;
        free(tofree);
    }
    if (i != noOfpids) {
        printf("Some process details might've been missed [%d vs actual %u]\n", i, noOfpids);
    }
}

/**
 * Inserts a new process info node into the linked list, sorted by descending pssTotal.
 */
void addProcessInfo(Process_Info *addPInfo)
{
    Process_Info *addNode = (Process_Info*)malloc(sizeof(Process_Info));
    if (addNode) {
        Process_Info *tmp = headProcessInfo;
        Process_Info *prev = NULL;
        memcpy(addNode, addPInfo, sizeof(Process_Info));
        while (tmp) {
            if (tmp->pssTotal < addNode->pssTotal) {
                if (prev) {
                    prev->next = addNode;
                    addNode->next = tmp;
                }
                else {
                    headProcessInfo = addNode;
                    headProcessInfo->next = tmp;
                }
                return;
            }
            prev = tmp;
            tmp = tmp->next;
        }
        if (headProcessInfo) {
            prev->next = addNode;
        }
        else {
            headProcessInfo = addNode;
        }
    }
}

#ifdef TESTME
/**
 * Frees the linked list and prints each node (for test mode).
 */
void checkAndFree()
{
    Process_Info *tmp = headProcessInfo, *tofree;
    int i = 1;
    while (tmp) {
        printf("%d,%u,%lu\n",
                i++, tmp->pid, tmp->pssTotal);
        tofree = tmp;
        tmp = tmp->next;
        free(tofree);
    }
    headProcessInfo = NULL;
}

/**
 * Test function to validate linked list insertion order.
 */
void testList ()
{
    Process_Info add = {0};
    add.pid = 1;
    add.pssTotal = 10;
    addProcessInfo(&add);
    add.pid = 2;
    add.pssTotal = 20;
    addProcessInfo(&add);
    add.pid = 3;
    add.pssTotal = 30;
    addProcessInfo(&add);
    checkAndFree();

    add.pid = 1;
    add.pssTotal = 30;
    addProcessInfo(&add);
    add.pid = 2;
    add.pssTotal = 20;
    addProcessInfo(&add);
    add.pid = 3;
    add.pssTotal = 10;
    addProcessInfo(&add);
    checkAndFree();

    add.pid = 1;
    add.pssTotal = 10;
    addProcessInfo(&add);
    add.pid = 2;
    add.pssTotal = 30;
    addProcessInfo(&add);
    add.pid = 3;
    add.pssTotal = 20;
    addProcessInfo(&add);
    checkAndFree();
}
#endif

// -----------------------------
// Process Info Parsing
// -----------------------------

/**
 * Parses /proc/[pid]/smaps for a given process and accumulates memory statistics.
 * Returns 0 on success, 1 on failure.
 */
int getProcessInfos(unsigned pid)
{
    static unsigned linesToSkipForRss = 0;
    static unsigned linesToSkipForPss = 0;
    static unsigned linesToSkipForSharedClean = 0;
    static unsigned linesToSkipForDirtyPrivate = 0;
    static unsigned linesToSkipForSwapPss = 0;
    static unsigned linesToSkipForRollover = 0;
    static unsigned linesToSkipIfRss0 = 0; 
    
    unsigned linesSkippedForRss = 0;
    unsigned linesSkippedForPss = 0;
    unsigned linesSkippedForSharedClean = 0;
    unsigned linesSkippedForDirtyPrivate = 0;
    unsigned linesSkippedForSwapPss = 0;
    unsigned linesSkippedForRollover = 0;

    char tmp[128];
#ifdef TESTME
    if (testpid) {
        testpid = 0;
        memcpy(tmp, testSmap, 128);
        printf("Testing with %s\n", tmp);
    }
    else
#endif	
    sprintf(tmp, "/proc/%u/smaps", pid);
    FILE *smap = fopen(tmp, "r");
    if (smap) {
        unsigned lines_To_skip = linesToSkipForRss;
        unsigned skipped = 1; // Tracks current skips
        unsigned expect_rss = 1;
        unsigned expect_pss = 0;
        unsigned expect_shared_clean = 0;
        unsigned expect_private_dirty = 0;
        unsigned expect_swap_pss = 0;
        unsigned prev_pss = 0;

#ifdef TESTME
        memset(&processInfoTest, 0, sizeof(processInfoTest));
#endif
        while (fgets(tmp, 127, smap)) {
#ifdef TESTME
            unsigned test_rss = 0, test_pss = 0;
            unsigned test_shared_clean = 0, test_private_dirty = 0;
            unsigned test_swap_pss = 0;
            if (strstr(tmp, "Rss:")) {
                PRINT_DBG("%s\n", tmp);
                if (sscanf(tmp, "Rss: %u kB", &test_rss)) {
                    processInfoTest.rssTotal += test_rss;
                }
            }
            else if (strstr(tmp, "Pss:")) {
                PRINT_DBG("%s\n", tmp);
                if (sscanf(tmp, "Pss: %u kB", &test_pss)) {
                    processInfoTest.pssTotal += test_pss;
                }
            }
            else if (strstr(tmp, "Shared_Clean:")) {
                PRINT_DBG("%s\n", tmp);
                if (sscanf(tmp, "Shared_Clean: %u kB", &test_shared_clean)) {
                    processInfoTest.shared_clean_total += test_shared_clean;
                }
            }
            else if (strstr(tmp, "Private_Dirty:")) {
                PRINT_DBG("%s\n", tmp);
                if (sscanf(tmp, "Private_Dirty: %u kB", &test_private_dirty)) {
                    processInfoTest.private_dirty_total += test_private_dirty;
                }
            }
            else if (strstr(tmp, "SwapPss:")) {
                PRINT_DBG("%s\n", tmp);
                if (sscanf(tmp, "SwapPss: %u kB", &test_swap_pss)) {
                    processInfoTest.swap_pss_total += test_swap_pss;
                }
            }
#endif
 			/*00010000-00041000 r-xp 00000000 fc:00 5600       /usr/sbin/dropbearmulti
  	 		  Size:                196 kB
  	 		  Rss:                 192 kB
  	 		  Pss:                  96 kB
  	 		  Shared_Clean:        192 kB
  	 		  Shared_Dirty:          0 kB
   	  	 	  Private_Clean:         0 kB
  	 		  Private_Dirty:         0 kB
  	 		  Referenced:          192 kB
  	 		  Anonymous:             0 kB
  	 		  AnonHugePages:         0 kB
  	 		  ShmemPmdMapped:        0 kB
   	  	 	  Shared_Hugetlb:        0 kB
  	 		  Private_Hugetlb:       0 kB
  	 		  Swap:                  0 kB
  	 		  SwapPss:               0 kB
  	 		  KernelPageSize:        4 kB
  	 		  MMUPageSize:           4 kB
   	  	 	  Locked:                0 kB
  	 		  VmFlags: rd ex mr mw me dw
  	 		  00050000-00051000 r--p 00030000 fc:00 5600       /usr/sbin/dropbearmulti
  	 		  Size:                  4 kB
  	 		  Rss:                   4 kB
  	 		  Pss:                   4 kB
   	  	 	  Shared_Clean:          0 kB
  	 		  Shared_Dirty:          0 kB
  	 		  Private_Clean:         0 kB
  	 		  Private_Dirty:         4 kB
 			*/
  	
			unsigned rss = 0, pss = 0;
			unsigned shared_clean = 0, private_dirty = 0;
			unsigned swap_pss = 0;
			if (linesToSkipForRollover) { // Learnt the format
				if (++skipped > lines_To_skip) {
					if (expect_rss) {
						//rss = 0;
						if (sscanf(tmp, "Rss: %u kB", &rss)) {
							if (rss) {
								PRINT_DBG_SCANNED("Read Rss (%u) after %u/%u lines --> %s\r", rss, skipped, lines_To_skip, tmp);
								getProcessInfo.rssTotal += rss;
								PRINT_DBG_SCANNED("rssTotal (%lu)\n", getProcessInfo.rssTotal);
								lines_To_skip = linesToSkipForPss;
								skipped = 1;
								expect_pss = 1;
								expect_rss = 0;
								continue;
							}
							else {
								lines_To_skip = linesToSkipIfRss0;
								skipped = 1;
								continue;
							}
						}
					} 
					else if (expect_pss) {
						//pss = 0;
						if (sscanf(tmp, "Pss: %u kB", &pss)) {
							PRINT_DBG_SCANNED("Read Pss (%u) after %u/%u lines  --> %s\r", pss, skipped, lines_To_skip, tmp);
							getProcessInfo.pssTotal += pss;
							prev_pss = pss;
							lines_To_skip = linesToSkipForSharedClean;
							skipped = 1;
							expect_shared_clean = 1;
							expect_pss = 0;
							continue;
						}
					}
					else if (expect_shared_clean) {
						//shared_clean = 0;
						if (sscanf(tmp, "Shared_Clean: %u kB", &shared_clean)) {
							PRINT_DBG_SCANNED("Read shared_clean (%u)  %u/%u lines --> %s\r", shared_clean, skipped, lines_To_skip, tmp);
							getProcessInfo.shared_clean_total += shared_clean;

							if (!prev_pss) {
								// No need to look at private dirty and swap pss
								expect_rss = 1;

								lines_To_skip = linesToSkipForSwapPss? linesToSkipForDirtyPrivate + linesToSkipForSwapPss + linesToSkipForRollover - 2 :
									linesToSkipForDirtyPrivate + linesToSkipForRollover - 1;
							}
							else {
								expect_private_dirty = 1;
								lines_To_skip = linesToSkipForDirtyPrivate;
							}
							expect_shared_clean = 0;
							skipped = 1;
							continue;
						}
					}
					else if (expect_private_dirty) {
						//private_dirty = 0;
						if (sscanf(tmp, "Private_Dirty: %u kB", &private_dirty)) {
							expect_private_dirty = 0;
							PRINT_DBG_SCANNED("Read private_dirty (%u)  %u/%u lines --> %s\r", private_dirty, skipped, lines_To_skip, tmp);
							getProcessInfo.private_dirty_total += private_dirty;
							skipped = 1;
							if (linesToSkipForSwapPss) {
								expect_swap_pss = 1;
								lines_To_skip = linesToSkipForSwapPss;
							}
							else {
								expect_rss = 1;
								lines_To_skip = linesToSkipForRollover;
							}
							continue;
						}
					}
					else if (expect_swap_pss) {
						//swap_pss = 0;
						if (sscanf(tmp, "SwapPss: %u kB", &swap_pss)) {
							expect_swap_pss = 0;
							expect_rss = 1;
							PRINT_DBG_SCANNED("Read swap_pss (%u)  %u/%u lines --> %s\r", swap_pss, skipped, lines_To_skip, tmp);
							getProcessInfo.swap_pss_total += swap_pss;
							lines_To_skip = linesToSkipForRollover;
							skipped = 1;
							continue;
						}
					}
				}
				else {
					continue;
				}
			}
			else { // Learn here
				if (!linesToSkipForRss) {
					if (sscanf(tmp, "Rss: %u kB", &rss)) {
						getProcessInfo.rssTotal += rss;
						linesToSkipForRss = linesSkippedForRss + 1;
						PRINT_DBG_INITIAL("After %u lines Read Rss (%u) in --> %s\r", linesToSkipForRss, rss, tmp);
						continue;
					}
					else {
						linesSkippedForRss++;
					}
				}
				else if (!linesToSkipForPss) {
					if (sscanf(tmp, "Pss: %u kB", &pss)) {
						getProcessInfo.pssTotal += pss;
						linesToSkipForPss = linesSkippedForPss + 1;
						PRINT_DBG_INITIAL("After %u lines Read Pss (%u) in --> %s\r", linesToSkipForPss, pss, tmp);
						continue;
					}
					else {
						linesSkippedForPss++;
					}
				}
				else if (!linesToSkipForSharedClean) {
					if (sscanf(tmp, "Shared_Clean: %u kB", &shared_clean)) {
						getProcessInfo.shared_clean_total += shared_clean;
						linesToSkipForSharedClean = linesSkippedForSharedClean + 1;
						PRINT_DBG_INITIAL("After %u lines Read Shared_Clean (%u) in --> %s\r", linesToSkipForSharedClean, shared_clean, tmp);
						continue;
					}
					else {
						linesSkippedForSharedClean++;
					}
				}
				else if (!linesToSkipForDirtyPrivate) {
					if (sscanf(tmp, "Private_Dirty: %u kB", &private_dirty)) {
						getProcessInfo.private_dirty_total += private_dirty;
						linesToSkipForDirtyPrivate = linesSkippedForDirtyPrivate + 1;
						PRINT_DBG_INITIAL("After %u lines Read DirtyPrivate (%u) in --> %s\r", linesToSkipForDirtyPrivate, private_dirty, tmp);
						continue;
					}
					else {
						linesSkippedForDirtyPrivate++;
					}
				}
				else if (!linesToSkipForSwapPss) {
					if (sscanf(tmp, "SwapPss: %u kB", &swap_pss)) {
						getProcessInfo.swap_pss_total += swap_pss;
						linesToSkipForSwapPss = linesSkippedForSwapPss + 1;
						PRINT_DBG_INITIAL("After %u lines Read Swap_Pss (%u) in --> %s\r", linesToSkipForSwapPss, swap_pss, tmp);
						continue;
					}
					else {
						// check if smap doesn't contain swappss..
						if (sscanf(tmp, "Rss: %u kB", &rss)) {
							getProcessInfo.rssTotal += rss;
							linesToSkipForRollover = linesSkippedForSwapPss + 1;
							PRINT_DBG_INITIAL("*****No SwapPss....skipping\n");
							PRINT_DBG_INITIAL("After %u lines Read Rss (%u) in --> %s\r", linesToSkipForRollover, rss, tmp);
							lines_To_skip = linesToSkipForPss;
							expect_pss = 1;
							expect_rss = 0;
							linesToSkipIfRss0 = linesToSkipForPss + linesToSkipForSharedClean + linesToSkipForDirtyPrivate + linesToSkipForRollover - 3;
							continue;
						}
						else {
							linesSkippedForSwapPss++;
						}
					}
				}
				else if (!linesToSkipForRollover) {
					if (sscanf(tmp, "Rss: %u kB", &rss)) {
						getProcessInfo.rssTotal += rss;
						linesToSkipForRollover = linesSkippedForRollover + 1;
						PRINT_DBG_INITIAL("After %u lines Read Rss (%u) in --> %s\r", linesToSkipForRollover, rss, tmp);
						lines_To_skip = linesToSkipForPss;
						expect_pss = 1;
						expect_rss = 0;
						linesToSkipIfRss0 = linesToSkipForPss + linesToSkipForSharedClean + linesToSkipForDirtyPrivate + linesToSkipForSwapPss + linesToSkipForRollover - 4;
						continue;
					}
					else {
						linesSkippedForRollover++;
					}
				}
				else {
					printf("%s:%d: Shouldn't get here..\n", __FUNCTION__, __LINE__);
				}
			}
		}
		fclose(smap);
		return 0;
	}
	else {
		printf("%s: Open failed, errno %d [%s]\n", tmp, errno, strerror(errno));
	}
	return 1;
}

/**
 * Prints usage information and exits.
 */
void printHelp(int argc, char *argv[])
{
    printf("%s [-a] [-o <output directory>] [-t <path to custom map>]\n", argv[0]);
    exit(1);
}

/**
 * Reads /proc/meminfo and writes the first 50 fields and their values to the output file.
 */
void saveMeminfo(FILE *out)
{
    unsigned long val[50];
    unsigned count = 0;
    char tmp[128];
    char name[64];

    FILE *meminfo = fopen("/proc/meminfo", "r");
    if (meminfo) {
        while (fgets(tmp, 127, meminfo) && count < 50) {
            if (!sscanf(tmp, "%s %lu kB", name, &val[count])) {
                printf("%s: Error parsing [%s]\n", __FUNCTION__, tmp);
            }
            else {
                name[strlen(name)-1] = '\0';
                if (count) {
                    fprintf(out, ",%s", name);
                }
                else {
                    fprintf(out, "/proc/meminfo:\n%s", name);
                }
                count++;
            }
        }
        fclose(meminfo);
    }
    for (int i=0; i<count; i++) {
        if (i) {
            fprintf(out, ",%lu", val[i]);
        }
        else {
            fprintf(out, "\n%lu", val[i]);
        }
    }
}

// -----------------------------
// Main Program
// -----------------------------

/**
 * Main entry point: parses arguments, scans processes, collects stats, writes output.
 */
int main(int argc, char *argv[])
{
	unsigned noOfPids = 0;
	char *outPath = NULL;
	//printf("%s with %d args\n", argv[0], argc);
	for (int i=1; i < argc; i++) {
		if (!strcmp(argv[i], "-o")) {
			if (i < argc + 1) {
				i++;
				DIR *outputDir = opendir(argv[i]);
				if (outputDir) {
					closedir(outputDir);
					outPath = argv[i];
					continue;
				}
				printf("Dir %s access error %d [%s]\n", argv[i], errno, strerror(errno));
			}
			printHelp(argc, argv);
		}
		if (!strcmp(argv[i], "-t")) {
			if (i < argc + 1) {
				i++;
				FILE *testMapFd = fopen(argv[i], "r");
				if (testMapFd) {
					fclose(testMapFd);
					testpid = 1;
					strncpy(testSmap, argv[i], 128);
					continue;
				}
				printf("Test map file %s open error %d [%s]\n", argv[i], errno, strerror(errno));
			}
			printHelp(argc, argv);
		}
		if (!strcmp(argv[i], "-a")) {
			includeKthreads = 1;
			continue;
		}
		//if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) 
		{
			printHelp(argc, argv);
		}
	}
#ifdef TESTME
	if (testpid) {
		if (getProcessInfos(0)) {
			if ((getProcessInfo.rssTotal != processInfoTest.rssTotal) || (getProcessInfo.pssTotal != processInfoTest.pssTotal) ||
					(getProcessInfo.shared_clean_total != processInfoTest.shared_clean_total) || (getProcessInfo.private_dirty_total != processInfoTest.private_dirty_total) ||
					(getProcessInfo.swap_pss_total != processInfoTest.swap_pss_total))
			{
				printf("something went wrong while processing %s\n", testSmap);
				printf("%lu:%lu, %lu:%lu, %lu:%lu, %lu:%lu, %lu:%lu\n", getProcessInfo.rssTotal, processInfoTest.rssTotal, getProcessInfo.pssTotal, 
						processInfoTest.pssTotal,getProcessInfo.shared_clean_total, processInfoTest.shared_clean_total, getProcessInfo.private_dirty_total, 
						processInfoTest.private_dirty_total, getProcessInfo.swap_pss_total, processInfoTest.swap_pss_total);
				pause();
			}
		}
		testList();
	}
#endif
	time_t timenow = time(NULL);
	struct tm *tmNow = localtime(&timenow);
	char filename[128] = {'\0'};
	char outputfile[256] = {'\0'};
	
	if (0 == strftime(filename, sizeof(filename), "%Y_%m_%d_%H_%M_%S_memstatus.txt", tmNow)) {
		// Shouldn't fail unless outputFilename is not big enough to hold
		sprintf(filename, "%lu_memstatus.txt", timenow); // see if this is warned in 32 bit systems..
	}
	sprintf(outputfile, "%s/%s", outPath?outPath:"/tmp/", filename);
	printf("time: %s\n", outputfile);
	FILE *output = fopen(outputfile, "w");

	if (NULL == output) {
		printf("%s: Open failed, %d [%s]\n", outputfile, errno, strerror(errno));
		exit(2);
	}
	unsigned long rssTotal = 0, pssTotal = 0, shared_clean_total = 0, private_dirty_total = 0, swap_pss_total = 0;
	DIR *proc = opendir("/proc");
	if (proc) {
		struct dirent *entry;
		while ((entry = readdir(proc)) != NULL) {
			if (entry->d_name[0] != '.') {
				unsigned pid = atoi(entry->d_name);
				char nameTmp[PATH_MAX]; // = {'\0'};
				if (pid) {
					/* readlink of /proc/pid/exe doesn't read all user space processes if user is not super user??
					 *
					int nbytes;
					char linkTmp[PATH_MAX] = {'\0'};
					sprintf(linkTmp, "/proc/%d/exe", pid);
					nbytes = readlink(linkTmp, pathTmp, PATH_MAX);
					if (-1 < nbytes)
						printf("%d: %s points to %s\n", nbytes, linkTmp, pathTmp);
					*
					*/

					/* Use /proc/pid/stat to read name, flags, stime, utime */
					char pathTmp[PATH_MAX];
					unsigned long utime, stime;
					unsigned long minFaults, majFaults;
					
					sprintf(pathTmp, "/proc/%d/stat", pid);
					FILE *fp = fopen(pathTmp, "r");
					if (fp) {
						unsigned flags;

						// pid comm state ppid pgrp session tty_nr tpgid flags minflt cminflt majflt cmajflt utime stime ...
						// 2 (kthreadd) S 0 0 0 0 -1 2129984 0 0 0 0 0 2
						if (6 == fscanf(fp, "%*d %*c%s %*c %*d %*d %*d %*d %*d %u %lu %*u %lu %*u %lu %lu", 
									    nameTmp, &flags, &minFaults, &majFaults, &utime, &stime )) {
							nameTmp[strlen(nameTmp)-1] = '\0';
							memset(&getProcessInfo, 0, sizeof(getProcessInfo));
							if (flags & 0x00200000) { /*PF_KTHREAD*/
								// Not needed to get smaps data..continue
								if (includeKthreads) {
									printf("%d,[%s],0,0,0,0,0,%lu,%lu\n", pid, nameTmp, majFaults, utime + stime);
									getProcessInfo.pid = pid;
									sprintf(getProcessInfo.name, "[%s]", nameTmp);
									getProcessInfo.majFaults = majFaults;
									getProcessInfo.cputime = utime + stime;
									addProcessInfo(&getProcessInfo);
								}
								fclose(fp);
								continue;
							}
							noOfPids++;
						}
						fclose(fp);
					}
					else {
						printf("%s: Open failed, errno %d [%s]\n", pathTmp, errno, strerror(errno));
						continue;
					}
					getProcessInfo.pid = pid;
					strcpy(getProcessInfo.name, nameTmp);
					getProcessInfo.majFaults = majFaults;
					getProcessInfo.cputime = utime + stime;
#ifdef TESTME
					memset(&processInfoTest, 0, sizeof(processInfoTest));
#endif
					if (!getProcessInfos(pid)) {
						rssTotal += getProcessInfo.rssTotal;
						pssTotal += getProcessInfo.pssTotal;
						shared_clean_total += getProcessInfo.shared_clean_total;
						private_dirty_total += getProcessInfo.private_dirty_total;
						swap_pss_total += getProcessInfo.swap_pss_total;
						addProcessInfo(&getProcessInfo);
					}
					else {
						printf("getProcessInfos failed..\n");
					}
#ifdef TESTME
					if ((getProcessInfo.rssTotal != processInfoTest.rssTotal) || (getProcessInfo.pssTotal != processInfoTest.pssTotal) ||
							(getProcessInfo.shared_clean_total != processInfoTest.shared_clean_total) || (getProcessInfo.private_dirty_total != processInfoTest.private_dirty_total) ||
							(getProcessInfo.swap_pss_total != processInfoTest.swap_pss_total))
					{
						printf("something went wrong while processing smap for pid %d\n", pid);
						printf("%lu:%lu, %lu:%lu, %lu:%lu, %lu:%lu, %lu:%lu\n", 
								getProcessInfo.rssTotal, processInfoTest.rssTotal,getProcessInfo.pssTotal,processInfoTest.pssTotal,getProcessInfo.shared_clean_total, processInfoTest.shared_clean_total, 
								getProcessInfo.private_dirty_total, processInfoTest.private_dirty_total, getProcessInfo.swap_pss_total, processInfoTest.swap_pss_total);
						pause();
					}
#endif
				}
			}
		}
		closedir(proc);
	}
	else {
		printf("/proc: Open failed, errno %d [%s]\n", errno, strerror(errno));
	}

	saveMeminfo(output);
	printf("Processed %u processes\nRSS_Total %lu\nPSS_Total %lu\nShared_Clean_Total %lu\nPrivate_Dirty_Total %lu\n", noOfPids, rssTotal, pssTotal, shared_clean_total, private_dirty_total);
	fprintf(output, "\n\nProcesses:\nPID,EXE,RSS,PSS,SHARED_CLEAN,PRIVATE_DIRTY,SWAP_PSS,MAJ_FAULTS,CPU_TIME\n");	
	writeProcessInfo(noOfPids, output);
	fprintf(output, "0,Total,%lu,%lu,%lu,%lu\n", rssTotal, pssTotal, shared_clean_total, private_dirty_total);
	fclose(output);
	return 0;
}
