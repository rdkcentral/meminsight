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
 *   ./xMemInsight [-a] [-c <config file>] [-t] [-h]
 *
 * Author: Jagadheesan Duraisamy
 * Date: 09/07/2025
 */

#include "memstatus.h"

// -----------------------------
// Global Variables
// -----------------------------
int includeKthreads = 0;           // Whether to include kernel threads
Process_Info getProcessInfo = {0}; // Temporary struct for collecting process info
Process_Info *headProcessInfo = NULL; // Head of linked list

#ifdef TESTME
int testpid;
char testSmap[128];
Process_Info processInfoTest;
#endif

// -----------------------------
// Utility Functions
// -----------------------------
/**
 * Reads a property value from a file in "key=value" format.
 * Returns 1 if found, 0 otherwise.
 */
int getPropertyFromFile(const char *filename, const char *property, char *propertyValue, size_t propertyValueLen) {
	FILE *fp = fopen(filename, "r");
	if (!fp) {
		if (propertyValue && propertyValueLen > 0) propertyValue[0] = '\0';
		return 0;
	}
	char line[256];
	int found = 0;
	while (fgets(line, sizeof(line), fp)) {
		char *trim = line;
		while (*trim == ' ' || *trim == '\t') trim++;	// Skip comments and empty lines
		if (*trim == '#' || *trim == '\0' || *trim == '\n') continue;	// Check for property match at start of line

		// Split on '='
		char *key = strtok(trim, "=");
		char *val = strtok(NULL, "\n");
		if (!key || !val) continue;

		// Remove trailing spaces from key
		char *kend = key + strlen(key) - 1;
		while (kend > key && (*kend == ' ' || *kend == '\t')) *kend-- = '\0';

		// Remove leading spaces from val
		while (*val == ' ' || *val == '\t') val++;

		if (strcmp(key, property) == 0) {
			// Find the length of the value, excluding any trailing newline or carriage return
			size_t valueLen = strcspn(val, "\r\n");
			// Limit the length to fit in the provided buffer
			if (valueLen >= propertyValueLen) {
				valueLen = propertyValueLen - 1;
			}
			// Copy the value into the output buffer
			strncpy(propertyValue, val, valueLen);
			propertyValue[valueLen] = '\0';
			found = 1;
			break;
		}
	}
	fclose(fp);
	if (!found && propertyValue && propertyValueLen > 0) propertyValue[0] = '\0';
	return found;
}

/**
 * Retrieves the MAC address for a given network interface.
 * Returns the number of characters written to macAddress.
 */
size_t getMacAddress(const char* iface, char *macAddress, size_t szBufSize) {
	if (!iface || !macAddress || szBufSize < 18) { // 17 chars + null
		printf("Invalid parameter\n");
		return 0;
	}
	struct ifreq ifr;
	int fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd == -1) {
		printf("socket create failed: %s\n", strerror(errno));
		return 0;
	}
	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1);
	ifr.ifr_addr.sa_family = AF_INET;
	if (ioctl(fd, SIOCGIFHWADDR, &ifr) == -1) {
		printf("ioctl SIOCGIFHWADDR failed: %s\n", strerror(errno));
		close(fd);
		return 0;
	}
	close(fd);

	unsigned char *mac = (unsigned char *)ifr.ifr_hwaddr.sa_data;
	size_t ret = snprintf(macAddress, szBufSize, "%02x:%02x:%02x:%02x:%02x:%02x",
	                      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	return ret;
}

/**
 * Checks if a string represents a valid PID (all digits).
 */
int isPID(const char *str) {
	if (!str || !*str) return 0;
	return strspn(str, "0123456789") == strlen(str);
}

/**
 * Finds the PID of a process by its name.
 * Returns 1 if found, 0 otherwise.
 */
int getPIDByProcessName(const char *procName, unsigned int *pidOut) {
	DIR *proc = opendir("/proc");
	if (!proc) return 0;
	struct dirent *entry;
	char commPath[PATH_MAX];
	char nameBuf[PATH_MAX];
	int found = 0;
	while ((entry = readdir(proc)) != NULL) {
		if (entry->d_type != DT_DIR) continue;
		if (!isPID(entry->d_name)) continue;
		snprintf(commPath, sizeof(commPath), "/proc/%s/comm", entry->d_name);
		FILE *f = fopen(commPath, "r");
		if (!f) continue;
		if (fgets(nameBuf, sizeof(nameBuf), f)) {
			// Remove trailing newline
			char *nl = strchr(nameBuf, '\n');
			if (nl) *nl = '\0';
			if (strcmp(nameBuf, procName) == 0) {
				*pidOut = (unsigned int)atoi(entry->d_name);
				found = 1;
				fclose(f);
				break;
			}
		}
		fclose(f);
	}
	closedir(proc);
	return found ? 1 : 0;
}

/**
 * Fills process statistics fields from /proc/[pid]/stat.
 * Returns 1 on success, 0 on failure.
 */
int fillProcessStatFields(unsigned pid, Process_Info *info, unsigned *flagsOut) {
	char statPath[PATH_MAX];
	snprintf(statPath, sizeof(statPath), "/proc/%u/stat", pid);
	FILE *statFile = fopen(statPath, "r");
	if (!statFile) {
		printf("Failed to open stat file for PID %u: %s\n", pid, strerror(errno));
		return 0;
	}
	char line[1024];
	if (!fgets(line, sizeof(line), statFile)) {
		fclose(statFile);
		return 0;
	}
	fclose(statFile);
	// Parsing Name
	char *open = strchr(line, '(');
	char *close = strrchr(line, ')');
	if (open && close && close > open + 1) {
		size_t len = close - open - 1;
		if (len >= sizeof(info->name)) len = sizeof(info->name) - 1;
		strncpy(info->name, open + 1, len);
		info->name[len] = '\0';
	} else {
		strcpy(info->name, "unknown");
	}
	// Parse other fields: flags (8), minflt (10), majflt (12), utime (14), stime (15)
	unsigned flags = 0, minflt = 0, majflt = 0;
	unsigned long utime = 0, stime = 0;
	char *after = close ? close + 2 : line;
	sscanf(after, "%*c %*d %*d %*d %*d %*d %u %u %*u %u %*u %lu %lu",
	       &flags, &minflt, &majflt, &utime, &stime);
	info->majFaults = majflt;
	info->cputime = utime + stime;
	if (flagsOut) *flagsOut = flags;
	return 1;
}

// -----------------------------
// Configuration Data
// -----------------------------

/**
 * Parses a configuration file for whitelist, output file, iterations, interval, and log level.
 * Returns 0 on success, -1 on failure.
 */
int parseConfig(const char *configPath, Config_Data *config) {
	const char *dot = strrchr(configPath, '.');
	if (!dot || (strcmp(dot, ".txt") != 0 && strcmp(dot, ".cfg") != 0 && strcmp(dot, ".conf") != 0)) {
		printf("Invalid config file format: %s\n", configPath);
		return -1;
	}

	config->whitelist = NULL;
	config->whiteListCount = 0;
	//config->outputFile = NULL;
	config.outDir[0] = '\0';
	config->iterations = DEFAULT_ITERATIONS; // Default to 1 iteration
	config->interval = DEFAULT_INTERVAL; // Default to 0 seconds interval
	strncpy(config->logLevel, DEFAULT_LOG_LEVEL, sizeof(config->logLevel) - 1);

	FILE *fp = fopen(configPath, "r");
	if (!fp) {
		printf("Failed to open config file: %s\n", configPath);
		return -1;
	}
	char line[512];
	while (fgets(line, sizeof(line), fp)) {
		char *eq = strchr(line, '=');
		if (!eq) continue;
		*eq = '\0';
		char *key = line;
		char *val = eq + 1;
		// Remove trailing newline and commas
		char *end = val + strlen(val) - 1;
		while (end > val && (*end == '\n' || *end == ',')) *end-- = '\0';

		if (strcmp(key, "process_whitelist") == 0) {
			// Comma separated
			int count = 1;
			for (char *p = val; *p; p++) if (*p == ',') count++;
			config->whiteListCount = count;
			config->whitelist = (char **)calloc(count, sizeof(char *));
			int idx = 0;
			char *tok = strtok(val, ",");
			while (tok && idx < count) {
				while (*tok == ' ') tok++;
				config->whitelist[idx++] = strdup(tok);
				tok = strtok(NULL, ",");
			}
		}
		/*
		else if (strcmp(key, "output_file") == 0) {
			config->outputFile = strdup(val);
			if (!config->outputFile) {
				char mac[32] = {0};
				static char defaultFile[256];
				getMacAddress(INTERFACE, mac, sizeof(mac));
				if (mac[0] == '\0') strcpy(mac, DEFAULT_MAC);
				time_t timenow = time(NULL);
				snprintf(defaultFile, sizeof(defaultFile), "/tmp/%s_%lu_%s", mac, timenow, CSV_FILE_NAME);
				config->outputFile = defaultFile;
			}
		}
		*/
		else if (strcmp(key, "output_dir") == 0) {
			strncpy(config->outputDir, val, PATH_MAX-1);
			config->outputDir[PATH_MAX-1] = '\0';
		}
		else if (strcmp(key, "iterations") == 0) {
			config->iterations = atoi(val);
		} else if (strcmp(key, "interval") == 0) {
			config->interval = atoi(val);
		} else if (strcmp(key, "log_level") == 0) {
			strncpy(config->logLevel, val, sizeof(config->logLevel) - 1);
			config->logLevel[sizeof(config->logLevel) - 1] = '\0';
		}
	}
	fclose(fp);
	return 0;
}

// -----------------------------
// Linked List Operations
// -----------------------------

/**
 * Writes all process info from the linked list to stdout and the output file.
 * Frees each node after writing.
 */
void writeProcessInfo(unsigned noOfPids, FILE *output)
{
	Process_Info *tmp = headProcessInfo;
	Process_Info *tofree;
	unsigned int i = 1;
	while (tmp) {
		printf("%d,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%u,%s\n",
		       i++, tmp->rssTotal, tmp->pssTotal, tmp->shared_clean_total, tmp->private_dirty_total, tmp->swap_pss_total, tmp->majFaults, tmp->cputime, tmp->pid, tmp->name);
		fprintf(output, "%u,%s,%lu,%lu,%lu,%lu,%lu,%lu,%lu\n",
		        tmp->pid, tmp->name, tmp->rssTotal, tmp->pssTotal, tmp->shared_clean_total, tmp->private_dirty_total, tmp->swap_pss_total, tmp->majFaults, tmp->cputime);
		tofree = tmp;
		tmp = tmp->next;
		free(tofree);
	}
	if (i != noOfPids) {
		printf("Some process details might've been missed [%d vs actual %u]\n", i, noOfPids);
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
void printHelp(char *argv[])
{
	printf("%s [-a] [-o <output directory>] [-t <path to custom map>]\n", argv[0]);
	exit(1);
}

/**
 * Prints detailed help and usage information and exits.
 */
void printHelpAndUsage(char *argv[], bool moreInfo)
{
	printf("Usage: %s [OPTIONS]\n", argv[0]);
	printf("A lightweight, configurable tool for collecting detailed system and per-process memory and CPU statistics.\n");

	printf("Options:\n");
	printf("  -a, --all					Include kernel threads for process monitoring\n");
	printf("  -c, --config <dir>		Path to configuration directory with %s extension\n", CONFIG_EXTN);
	printf("  -o, --output <directory>	Output directory for generated CSV files (default: /tmp)\n");
	printf("      --interval <seconds>	Interval in seconds between iterations (overrides config)\n");
	printf("      --iterations <count>	Number of iterations to run (overrides config)\n");
	printf("  -h, --help				Show this help message and exit\n");
	printf("  -t, --test				Run in test mode with a generated minimal config\n");
	printf("\n");

	if (moreInfo) {
		printf("\nPrecedence:\n");
		printf("  Command-line arguments override config file values, which override built-in defaults.\n");
		printf("\nDefault behavior (no flags):\n");
		printf("  - Runs 1 iteration, interval 0s, monitors all processes, log level INFO\n");
		printf("  - Output: /tmp/<MAC>_<timestamp>_%s\n", CSV_FILE_NAME);
		printf("\n");

		printf("Example:\n");
		printf("  %s\n", argv[0]);
		printf("  %s -a\n", argv[0]);
		printf("  %s --config /etc/xmem_configuration.%s\n", argv[0], CONFIG_EXTN);
		printf("  %s -c myconfig.%s -a --interval 10 --iterations 5\n", argv[0], CONFIG_EXTN);
		printf("  %s --output /var/log/ --iterations 3\n", argv[0]);
		printf("  %s --test\n", argv[0]);
		printf("\n");
		printf("Sample config file:\n");
		printf("\n  process_whitelist=myapp,systemd,1234,\n  output_dir=/var/log,\n  iterations=10,\n  interval=60\n  log_level=INFO\n");

		printf("\nNotes:\n");
		printf("  - Supported config file extensions: %s\n", CONFIG_EXTN);
		printf("  - If both interval and iterations are set via CLI, both are used.\n");
		printf("  - If only one is set, the other uses its default or config value.\n");
		printf("  - Output file name format: <MAC>_<TIMESTAMP>_%s\n", CSV_FILE_NAME);
	}
	exit(1);
}

/**
 * Collects system-wide memory statistics and writes to output file.
 * Returns 0 on success, -1 on failure.
 */
int collectSystemMemoryStats(bool includeKthreads, const char *outDir, int iterations, int interval) {
	for (int iter = 0; iter < iterations; iter++) {
		unsigned int noOfPids = 0;

		// MAC Address
		char mac[32] = {0};
		getMacAddress(INTERFACE, mac, sizeof(mac));
		if (mac[0] == '\0') {
			strcpy(mac, DEFAULT_MAC);
		}

		// Current timestamp
		time_t timenow = time(NULL);
		struct tm *tm_info = localtime(&timenow);
		char timestamp[128] = {0};
		// Format: YYYYMMDD_HHMMSS
		strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", tm_info);

		// outDir or default /tmp/
		const char *dir = (outDir && outDir[0]) ? outDir : DEFAULT_OUT_DIR;
		char outputfile[512] = {0};
		snprintf(outputfile, sizeof(outputfile), "%s/%s_%s_%s", dir, mac, timestamp, CSV_FILE_NAME); // add iteration number here if required

		printf("System-wide monitoring output: %s\n", outputfile);
		FILE *output = fopen(outputfile, "w");
		if (NULL == output) {
			printf("%s: Open failed, %d [%s]\n", outputfile, errno, strerror(errno));
			return -1;
		}

		unsigned long rssTotal = 0, pssTotal = 0, shared_clean_total = 0, private_dirty_total = 0, swap_pss_total = 0;
		DIR *proc = opendir(PROC_DIR);
		if (proc) {
			struct dirent *entry;
			while ((entry = readdir(proc)) != NULL) {
				if (entry->d_name[0] != '.') {
					unsigned pid = atoi(entry->d_name);
					if (pid > 0) {
						unsigned flags = 0;
						memset(&getProcessInfo, 0, sizeof(getProcessInfo));
						if (!fillProcessStatFields(pid, &getProcessInfo, &flags)) {
							printf("Failed to fill process stat fields for PID %d\n", pid);
							continue;
						}
						if (flags & PF_KTHREAD && !includeKthreads) { /*PF_KTHREAD*/
							continue; // Skip kernel threads if not included
						}
						if (!getProcessInfos(pid)) {
							getProcessInfo.pid = pid;
							// name, majFaults, cputime already filled by fillProcessStatFields
							rssTotal += getProcessInfo.rssTotal;
							pssTotal += getProcessInfo.pssTotal;
							shared_clean_total += getProcessInfo.shared_clean_total;
							private_dirty_total += getProcessInfo.private_dirty_total;
							swap_pss_total += getProcessInfo.swap_pss_total;
							addProcessInfo(&getProcessInfo);
							noOfPids++;
						}
					}
				}
			}
			closedir(proc);
		} else {
			printf("Failed to open %s directory: %s\n", PROC_DIR, strerror(errno));
			fclose(output);
			return -1;
		}

		saveMeminfo(output);
		printf("Processed %u processes\nRSS_Total %lu\nPSS_Total %lu\nShared_Clean_Total %lu\nPrivate_Dirty_Total %lu\n",
			noOfPids, rssTotal, pssTotal, shared_clean_total, private_dirty_total);
		fprintf(output, "\n\nProcesses:\nPID,EXE,RSS,PSS,SHARED_CLEAN,PRIVATE_DIRTY,SWAP_PSS,MAJ_FAULTS,CPU_TIME\n");
		writeProcessInfo(noOfPids, output);
		fprintf(output, "0,Total,%lu,%lu,%lu,%lu,%lu,0,0,0\n", rssTotal, pssTotal, shared_clean_total, private_dirty_total, swap_pss_total);
		fclose(output);

		if (interval > 0 && iter + 1 < iterations) {
			printf("Sleeping for %d seconds before next iteration...\n", interval);
			sleep(interval);
		}
	}
	return 0;
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
		int first = 1;
		// print header
		while (fgets(tmp, 127, meminfo) && count < 50) {
			if (sscanf(tmp, "%s %lu kB", name, &val[count]) == 2) {
				name[strlen(name)-1] = '\0'; // Remove trailing ':'
				if (!first) {
					fprintf(out, ",");
				}
				fprintf(out, "%s", name);
				first = 0;
				count++;
			}
		}
		fprintf(out, "\n");
		fseek(meminfo, 0, SEEK_SET); // Reset file pointer to start
		count = 0;
		first = 1;
		// Print values
		while (fgets(tmp, 127, meminfo) && count < 50) {
			if (sscanf(tmp, "%s %lu kB", name, &val[count]) == 2) {
				if (!first) {
					fprintf(out, ",");
				}
				fprintf(out, "%lu", val[count]);
				first = 0;
				count++;
			}
		}
		fprintf(out, "\n\n"); // Blank line after meminfo block
		fclose(meminfo);
	}
}

// -----------------------------
// Main Program
// -----------------------------

/**
 * Main entry point: parses arguments, scans processes, collects stats, writes output.
 */
int main(int argc, char *argv[]) {
	// set defaults
	bool isConfigPresent = false;
	bool enableKThreads = false;
	bool isTestMode = false;
	bool isSystemWide = false;
	// char confFile[PATH_MAX] = {0};
	const char *final_out_dir = DEFAULT_OUT_DIR;
	int cli_iterations = -1;
	int cli_interval = -1;

	// CLI parsing and initialization
	if (argc == 1) {
		isSystemWide = true;
	}

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-c") || !strcmp(argv[i], "--config")) { // config file
			if (i+1 < argc) {
				strncpy(confFile, argv[i + 1], PATH_MAX - 1);
				FILE *fp = fopen(confFile, "r");
				if (!fp) {
					printf("Error: Config file '%s' does not exist or cannot be opened.\n", confFile);
					printHelpAndUsage(argv, true);
				}
				fclose(fp);
				isConfigPresent = true;
				i++; // Skip next arg (conf file)
			} else {
				printf("Error: Missing config file path after %s\n", argv[i]);
				printHelpAndUsage(argv, false);
			}
		} else if (!strcmp(argv[i], "-a") || !strcmp(argv[i], "--all")) { // include kernel threads
			enableKThreads = true;
		} else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) { // help
			printHelpAndUsage(argv, true);
		} else if (!strcmp(argv[i], "-t") || !strcmp(argv[i], "--test")) { // test mode
			isTestMode = true;
		} else if (!strcmp(argv[i], "-o") || !strcmp(argv[i], "--output")) { // output directory
			if (i+1 < argc) {
				strncpy(out_dir, argv[i+1], PATH_MAX - 1);
				i++; // skip next arg (output directory)
			} else {
				printf("Error: Missing output directory path after %s\n", argv[i]);
				printHelpAndUsage(argv, false);
			}
		} else if (!strcmp(argv[i], "--interval")) { // interval
			if (i+1 < argc) {
				cli_interval = atoi(argv[i+1]);
				i++; // skip next arg (interval)
			} else {
				printf("Error: Missing interval value after %s\n", argv[i]);
				printHelpAndUsage(argv, false);
			}
		} else if (!strcmp(argv[i], "--iterations")) { // iterations
			if (i+1 < argc) {
				cli_iterations = atoi(argv[i+1]);
				i++; // skip next arg (iterations)
			} else {
				printf("Error: Missing iterations value after %s\n", argv[i]);
				printHelpAndUsage(argv, false);
			}
		} else {
			printf("Error: Unrecognized argument '%s'\n", argv[i]);
			printHelpAndUsage(argv, false);
		}
	}
	includeKthreads = enableKThreads; // Cascade to global for all code paths

	if (isConfigPresent) {
		Config_Data config;
		if (parseConfig(confFile, &config) != 0) {
			printf("Error: Failed to parse config file '%s'\n", confFile);
			return -1; // TODO: Need to handle this better
		}

		// Output directory: CLI > config > default
		const char *final_out_dir = (out_dir[0] && strcmp(out_dir, DEFAULT_OUT_DIR) != 0) ? out_dir:(config.outputDir[0] ? config.outputDir : DEFAULT_OUT_DIR);

		// Interval/Iterations logic (CLI > config > default)
		int final_iterations = config.iterations;
		int final_interval = config.interval;

		if (cli_iterations > 0 && cli_interval > 0) { // valid CLI values
			final_iterations = cli_iterations;
            final_interval = cli_interval;
        } else if (cli_iterations > 0 && (cli_interval <= 0)) { // only iterations is valid
            final_iterations = cli_iterations;
            final_interval = DEFAULT_INTERVAL;
        } else if ((cli_iterations <= 0) && cli_interval > 0) { // only interval is valid
            final_iterations = 2;
            final_interval = cli_interval;
        } else if ((cli_iterations <= 0) && (cli_interval <= 0)) { // no valid CLI values
            // Use config values if present, else defaults
            if (final_iterations <= 0) final_iterations = DEFAULT_ITERATIONS;
            if (final_interval <= 0) final_interval = DEFAULT_INTERVAL;
        }

		// Defensive: if config values are negative or zero, use defaults
		if (final_iterations <= 0) final_iterations = DEFAULT_ITERATIONS;
		if (final_interval < 0) final_interval = DEFAULT_INTERVAL;

		config.iterations = final_iterations;
		config.interval = final_interval;

		printf("%s Config loaded:\n whitelist=%p\n count=%u\n outDir=%s\n iterations=%u\n interval=%u\n logLevel=%s\n",
		       confFile, config.whitelist, config.whiteListCount, config.outputDir, final_iterations, final_interval, config.logLevel);

		for (int iter = 0; iter < final_iterations; iter++) {
			// Get MAC address
			char mac[32] = {0};
			getMacAddress(INTERFACE, mac, sizeof(mac));
			if (mac[0] == '\0') strcpy(mac, DEFAULT_MAC);

			// Get timestamp
			time_t timenow = time(NULL);
			struct tm *tm_info = localtime(&timenow);
			char timestamp[32] = {0};
			strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", tm_info);

			// Generate output file name
			char outputFilePath[PATH_MAX * 2] = {0};
			snprintf(outputFilePath, sizeof(outputFilePath), "%s/%s_%s_%s", final_out_dir, mac, timestamp, CSV_FILE_NAME);

			// Open output file
			FILE *output = fopen(outputFilePath, "w");
			if (!output) {
                printf("Error: Failed to open output file '%s' for writing\n", outputFilePath);
                for (unsigned j = 0; j < config.whiteListCount; j++) {
                    if (config.whitelist[j] != NULL) {
                        free(config.whitelist[j]);
                    }
                }
                if (config.whitelist != NULL) {
                    free(config.whitelist);
                }
                return -1;
            }

			saveMeminfo(output);
			fprintf(output, "\nPID,EXE,RSS,PSS,SHARED_CLEAN,PRIVATE_DIRTY,SWAP_PSS,MAJ_FAULTS,CPU_TIME\n");

			unsigned long rssTotal = 0, pssTotal = 0, shared_clean_total = 0, private_dirty_total = 0, swap_pss_total = 0;
			unsigned actualCount = 0;
			if (config.whiteListCount > 0) {
				for (unsigned j = 0; j < config.whiteListCount; j++) {
					unsigned int pid = 0;
					printf("Processing whitelist item %s\n", config.whitelist[j]);
					if (isPID(config.whitelist[j])) {
						pid = (unsigned int)atoi(config.whitelist[j]);
					} else {
						if (!getPIDByProcessName(config.whitelist[j], &pid)) {
							printf("Process name'%s' not found\n", config.whitelist[j]);
							continue; // Skip to next item
						}
					}
					memset(&getProcessInfo, 0, sizeof(getProcessInfo));
					unsigned flags = 0;
					if (!fillProcessStatFields(pid, &getProcessInfo, &flags)) {
						printf("Failed to fill process stat fields for PID %u\n", pid);
						continue; // Skip to next item
					}
					if (flags & PF_KTHREAD && !enableKThreads) { /*PF_KTHREAD*/
						printf("Skipping kernel thread PID %u\n", pid);
						continue; // Skip kernel threads if not included
					}
					if (getProcessInfos(pid) == 0) {
						getProcessInfo.pid = pid;
						// name, majFaults, cputime already filled by fillProcessStatFields
						rssTotal += getProcessInfo.rssTotal;
						pssTotal += getProcessInfo.pssTotal;
						shared_clean_total += getProcessInfo.shared_clean_total;
						private_dirty_total += getProcessInfo.private_dirty_total;
						swap_pss_total += getProcessInfo.swap_pss_total;
						addProcessInfo(&getProcessInfo);
						actualCount++;
					} else {
						printf("Failed to get process info for PID %u\n", pid);
					}
				}
				writeProcessInfo(actualCount, output);
				headProcessInfo = NULL; // Reset for next iteration
			} else {
				printf("No whitelist specified in config.\n");
			}
			fprintf(output, "0,Total,%lu,%lu,%lu,%lu\n", rssTotal, pssTotal, shared_clean_total, private_dirty_total);
			fclose(output);

			if (final_interval > 0 && iter + 1 < final_iterations) {
                printf("Sleeping for %d seconds before next iteration...\n", final_interval);
                sleep(final_interval);
            }
		}

		// Free the whitelist memory
		for (unsigned j = 0; j < config.whiteListCount; j++) {
			if (config.whitelist[j] != NULL) {
				free(config.whitelist[j]);
			}
		}
		if(config.whitelist != NULL) {
			free(config.whitelist);
		}
	} else if (isSystemWide) {
		int final_iterations = (cli_iterations > 0) ? cli_iterations : DEFAULT_ITERATIONS;
        int final_interval = (cli_interval > 0) ? cli_interval : DEFAULT_INTERVAL;
		collectSystemMemoryStats(enableKThreads, final_out_dir, final_iterations, final_interval);
	} else if (isTestMode) {
		printf("Running in test mode with minimal config...\n");
		printf("TO BE IMPLEMENTED"); // TODO: Implement test mode logic
	}
}

/**
 * Main entry point: parses arguments, scans processes, collects stats, writes output.
 */
#if 0
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
			printHelp(argv);
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
			printHelp(argv);
		}
		if (!strcmp(argv[i], "-a")) {
			includeKthreads = 1;
			continue;
		}
		//if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h"))
		{
			printHelp(argv);
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
#endif
