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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "memstatus.h"

#ifdef ENABLE_CJSON
#include <cjson/cJSON.h>
#endif

// -----------------------------
// Global Variables
// -----------------------------
int includeKthreads = 0;              // Whether to include kernel threads
Process_Info getProcessInfo = {0};    // Temporary struct for collecting process info
Process_Info *headProcessInfo = NULL; // Head of linked list
int g_processLimit = -1;              // Default to unlimited processes (-1)

// Initialize format based on compile-time setting
#if defined(DEFAULT_FORMAT_JSON)
#ifdef ENABLE_CJSON
OutputFormat g_outputFormat = FORMAT_JSON;
#else
OutputFormat g_outputFormat = FORMAT_CSV;
#endif
#elif defined(DEFAULT_FORMAT_CSV)
OutputFormat g_outputFormat = FORMAT_CSV;
#else
OutputFormat g_outputFormat = FORMAT_CSV; // Default to CSV if not specified
#endif

static const char deviceIdentifierName[] = DEVICE_IDENTIFIER;
static const char xMemInsightVersion[] = "" XMEMINSIGHT_MAJOR_VERSION "." XMEMINSIGHT_MINOR_VERSION "";
static const char reportVersion[] =  "" REPORT_MAJOR_VERSION "." REPORT_MINOR_VERSION "";

// -----------------------------
// Utility Functions
// -----------------------------

/**
 * Ensures that the specified output directory exists.
 * If it doesn't exist, it attempts to create it.
 */
static void ensure_output_dir(const char *dir)
{
    struct stat st = {0};
    if (stat(dir, &st) == -1)
    {
        if (mkdir(dir, 0755) == -1)
        {
            printf("Failed to create output directory '%s': %s\n", dir, strerror(errno));
        }
    }
}

/**
 * Reads a property value from a file in "key=value" format.
 * Returns 1 if found, 0 otherwise.
 */
int getPropertyFromFile(const char *filename, const char *property, char *propertyValue, size_t propertyValueLen)
{
    FILE *fp = fopen(filename, "r");
    if (!fp)
    {
        if (propertyValue && propertyValueLen > 0)
            propertyValue[0] = '\0';
        return 0;
    }
    char line[256];
    int found = 0;
    while (fgets(line, sizeof(line), fp))
    {
        char *trim = line;
        while (*trim == ' ' || *trim == '\t')
            trim++; // Skip comments and empty lines
        if (*trim == '#' || *trim == '\0' || *trim == '\n')
            continue; // Check for property match at start of line

        // Split on '='
        char *key = strtok(trim, "=");
        char *val = strtok(NULL, "\n");
        if (!key || !val)
            continue;

        // Remove trailing spaces from key
        char *kend = key + strlen(key) - 1;
        while (kend > key && (*kend == ' ' || *kend == '\t'))
            *kend-- = '\0';

        // Remove leading spaces from val
        while (*val == ' ' || *val == '\t')
            val++;

        if (strncmp(key, property, strlen(property) + 1) == 0)
        {
            // Find the length of the value, excluding any trailing newline or
            // carriage return
            size_t valueLen = strcspn(val, "\r\n");
            // Limit the length to fit in the provided buffer
            if (valueLen >= propertyValueLen)
            {
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
    if (!found && propertyValue && propertyValueLen > 0)
        propertyValue[0] = '\0';
    return found;
}

/**
 * Reads the firmware image name from /version.txt.
 * Returns 1 if found, 0 otherwise. Writes result to fwName buffer.
 */

 int getFirmwareImageName(char *fwName, size_t fwNameLen)
 {
    if (!fwName || fwNameLen == 0)
    {
        printf("Invalid parameter for firmware name\n");
        return 0;
    }
    FILE *fp = fopen(VERSION_FILE, "r");
    if (!fp)
    {
        strncpy(fwName, DEFAULT_FW_NAME, fwNameLen - 1);
        fwName[fwNameLen - 1] = '\0';
        return 0;
    }
    char line[FW_LEN + 32] = {0};
    if (fgets(line, sizeof(line), fp))
    {
        char *p = strstr(line, "imagename:");
        if (p)
        {
            p += strlen("imagename:");
            while (*p == ' ' || *p == '\t') p++;
            size_t len = strcspn(p, "\r\n");
            if (len >= fwNameLen)
            {
                len = fwNameLen - 1;
            }
            strncpy(fwName, p, len);
            fwName[len] = '\0';
            fclose(fp);
            return 1;
        }
    }
    fclose(fp);
    strncpy(fwName, DEFAULT_FW_NAME, fwNameLen - 1);
    fwName[fwNameLen - 1] = '\0';
    return 0;
 }

/**
 * Retrieves the MAC address for a given network interface.
 * Returns the number of characters written to macAddress.
 */
size_t getMacAddress(const char *iface, char *macAddress, size_t szBufSize)
{
    if (!iface || !macAddress || szBufSize < 18)
    { // 17 chars + null
        printf("Invalid parameter\n");
        return 0;
    }
    struct ifreq ifr;
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1)
    {
        printf("socket create failed: %s\n", strerror(errno));
        return 0;
    }
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1);
    ifr.ifr_addr.sa_family = AF_INET;
    if (ioctl(fd, SIOCGIFHWADDR, &ifr) == -1)
    {
        printf("ioctl SIOCGIFHWADDR failed: %s\n", strerror(errno));
        close(fd);
        return 0;
    }
    close(fd);

    unsigned char *mac = (unsigned char *)ifr.ifr_hwaddr.sa_data;
    size_t ret = snprintf(macAddress, szBufSize, "%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return ret;
}

/**
 * Checks if a string represents a valid PID (all digits).
 */
int isPID(const char *str)
{
    if (!str || !*str)
        return 0;
    return strspn(str, "0123456789") == strlen(str);
}

/**
 * Finds the PID of a process by its name.
 * Returns 1 if found, 0 otherwise.
 */
int getPIDByProcessName(const char *procName, unsigned int *pidOut)
{
    DIR *proc = opendir("/proc");
    if (!proc)
        return 0;
    struct dirent *entry;
    char commPath[PATH_MAX];
    char nameBuf[PATH_MAX];
    int found = 0;
    while ((entry = readdir(proc)) != NULL)
    {
        if (entry->d_type != DT_DIR)
            continue;
        if (!isPID(entry->d_name))
            continue;
        snprintf(commPath, sizeof(commPath), "/proc/%s/comm", entry->d_name);
        FILE *f = fopen(commPath, "r");
        if (!f)
            continue;
        if (fgets(nameBuf, sizeof(nameBuf), f))
        {
            // Remove trailing newline
            char *nl = strchr(nameBuf, '\n');
            if (nl)
                *nl = '\0';
            if (strncmp(nameBuf, procName, strlen(procName) + 1) == 0)
            {
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
int fillProcessStatFields(unsigned pid, Process_Info *info, unsigned *flagsOut)
{
    char statPath[PATH_MAX];
    snprintf(statPath, sizeof(statPath), "/proc/%u/stat", pid);
    FILE *statFile = fopen(statPath, "r");
    if (!statFile)
    {
        printf("Failed to open stat file for PID %u: %s\n", pid, strerror(errno));
        return 0;
    }
    char line[1024];
    if (!fgets(line, sizeof(line), statFile))
    {
        fclose(statFile);
        return 0;
    }
    fclose(statFile);
    // Parsing Name
    char *open = strchr(line, '(');
    char *close = strrchr(line, ')');
    if (open && close && close > open + 1)
    {
        size_t len = close - open - 1;
        if (len >= sizeof(info->name))
            len = sizeof(info->name) - 1;
        strncpy(info->name, open + 1, len);
        info->name[len] = '\0';
    }
    else
    {
        strncpy(info->name, "unknown", sizeof(info->name) - 1);
        info->name[sizeof(info->name) - 1] = '\0';
    }
    // Parse other fields: flags (8), minflt (10), majflt (12), utime (14), stime
    // (15)
    unsigned flags = 0, minflt = 0, majflt = 0;
    unsigned long utime = 0, stime = 0;
    char *after = close ? close + 2 : line;
    sscanf(after, "%*c %*d %*d %*d %*d %*d %u %u %*u %u %*u %lu %lu", &flags, &minflt, &majflt, &utime, &stime);
    info->majFaults = majflt;
    info->cputime = utime + stime;
    if (flagsOut)
        *flagsOut = flags;
    return 1;
}

// -----------------------------
// Configuration Data
// -----------------------------

/**
 * Parses a configuration file for whitelist, output file, iterations,
 * interval, and log level. Returns 0 on success, -1 on failure.
 */
int parseConfig(const char *configPath, Config_Data *config)
{
    const char *dot = strrchr(configPath, '.');
    if (!dot || (strncmp(dot, ".conf", 6) != 0))
    {
        printf("Invalid config file format: %s\n", configPath);
        return -1;
    }

    config->whitelist = NULL;
    config->whiteListCount = 0;
    // config->outputFile = NULL;
    config->outputDir[0] = '\0';
    config->iterations = DEFAULT_ITERATIONS; // Default to 1 iteration
    config->interval = DEFAULT_INTERVAL;     // Default to 0 seconds interval
    strncpy(config->logLevel, DEFAULT_LOG_LEVEL, sizeof(config->logLevel) - 1);

    FILE *fp = fopen(configPath, "r");
    if (!fp)
    {
        printf("Failed to open config file: %s\n", configPath);
        return -1;
    }
    char line[512];
    while (fgets(line, sizeof(line), fp))
    {
        char *eq = strchr(line, '=');
        if (!eq)
            continue;
        *eq = '\0';
        char *key = line;
        char *val = eq + 1;
        // Remove trailing newline and commas
        char *end = val + strlen(val) - 1;
        while (end > val && (*end == '\n' || *end == ','))
            *end-- = '\0';

        if (strcmp(key, "process_whitelist") == 0)
        {
            // Comma separated
            int count = 1;
            for (char *p = val; *p; p++)
                if (*p == ',')
                    count++;
            config->whiteListCount = count;
            config->whitelist = (char **)calloc(count, sizeof(char *));
            int idx = 0;
            char *tok = strtok(val, ",");
            while (tok && idx < count)
            {
                while (*tok == ' ')
                    tok++;
                config->whitelist[idx++] = strdup(tok);
                tok = strtok(NULL, ",");
            }
        }
        else if (strcmp(key, "output_dir") == 0)
        {
            strncpy(config->outputDir, val, PATH_MAX - 1);
            config->outputDir[PATH_MAX - 1] = '\0';
        }
        else if (strcmp(key, "iterations") == 0)
        {
            config->iterations = atoi(val);
        }
        else if (strcmp(key, "interval") == 0)
        {
            config->interval = atoi(val);
        }
        else if (strcmp(key, "log_level") == 0)
        {
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
    unsigned int processCount = 0;
    while (tmp)
    {
        // Only write up to g_processLimit if set
        if (g_processLimit > 0 && processCount >= (unsigned)g_processLimit)
        {
            tofree = tmp;
            tmp = tmp->next;
            free(tofree);
            continue;
        }
        //printf("%d,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%u,%s\n", i++, tmp->rssTotal, tmp->pssTotal, tmp->shared_clean_total,tmp->private_dirty_total, tmp->swap_pss_total, tmp->majFaults, tmp->cputime, tmp->pid, tmp->name);
        fprintf(output, "%u,%s,%lu,%lu,%lu,%lu,%lu,%lu,%lu\n", tmp->pid, tmp->name, tmp->rssTotal, tmp->pssTotal,
                tmp->shared_clean_total, tmp->private_dirty_total, tmp->swap_pss_total, tmp->majFaults, tmp->cputime);
        tofree = tmp;
        tmp = tmp->next;
        free(tofree);
        processCount++;
        i++;
    }
    headProcessInfo = NULL;
    if (i != noOfPids + 1 && (g_processLimit < 0 || processCount < (unsigned)g_processLimit))
    {
        printf("* Some process details might have been missed [%d vs actual %u]%s\n", i-1, noOfPids, g_processLimit > 0 ? " (limited by -n option)" : "");
    }
}

#ifdef ENABLE_CJSON
/**
 * Adds process information from the linked list to the root JSON object
 * and frees the linked list nodes as it processes them.
 *
 * @param root The root JSON object to add processes to
 * @param noOfPids Number of PIDs processed
 * @param output FILE pointer for diagnostic messages (if needed)
 */
void addProcessInfoJSON(cJSON *root, unsigned noOfPids, FILE *output)
{
    if (!root) return;

    Process_Info *tmp = headProcessInfo;
    Process_Info *tofree;
    unsigned int i = 1;
    unsigned int processCount = 0;

    // Create the JSON array for processes
    cJSON *processesArray = cJSON_CreateArray();
    if (!processesArray) {
        fprintf(output, "\"processes\": null,\n");
        // Still need to free the linked list
        while (tmp) {
            tofree = tmp;
            tmp = tmp->next;
            free(tofree);
        }
        headProcessInfo = NULL;
        return;
    }
    // Process each node in the linked list
    while (tmp) {
        // Only output up to g_processLimit processes if set
        if (g_processLimit > 0 && processCount >= (unsigned)g_processLimit) {
            tofree = tmp;
            tmp = tmp->next;
            free(tofree);
            continue;
        }

        // Create a JSON object for this process
        cJSON *process = cJSON_CreateObject();
        if (process) {
            // Add process data
            cJSON_AddNumberToObject(process, "pid", tmp->pid);
            cJSON_AddStringToObject(process, "name", tmp->name);
            cJSON_AddNumberToObject(process, "rss", tmp->rssTotal);
            cJSON_AddNumberToObject(process, "pss", tmp->pssTotal);
            cJSON_AddNumberToObject(process, "shared_clean", tmp->shared_clean_total);
            cJSON_AddNumberToObject(process, "private_dirty", tmp->private_dirty_total);
            cJSON_AddNumberToObject(process, "swap_pss", tmp->swap_pss_total);
            cJSON_AddNumberToObject(process, "maj_faults", tmp->majFaults);
            cJSON_AddNumberToObject(process, "cpu_time", tmp->cputime);

            // Add to the array
            cJSON_AddItemToArray(processesArray, process);
        }

        tofree = tmp;
        tmp = tmp->next;
        free(tofree);
        processCount++;
        i++;
    }
    // Reset the head pointer
    headProcessInfo = NULL;

    // Check if we potentially missed processes
    if (i != noOfPids + 1 && (g_processLimit < 0 || processCount < (unsigned)g_processLimit)) {
        printf("* Some process details might have been missed [%d vs actual %u]%s\n",
               i-1, noOfPids, g_processLimit > 0 ? " (limited by -n option)" : "");
    }
    cJSON_AddItemToObject(root, "processes", processesArray);
}

/**
 * Adds process totals to the root JSON object as "process_total"
 *
 * @param root The root JSON object
 * @param rssTotal Total RSS value
 * @param pssTotal Total PSS value
 * @param shared_clean_total Total shared clean value
 * @param private_dirty_total Total private dirty value
 * @param swap_pss_total Total swap PSS value
 */

void addProcessTotalsJSON(cJSON *root, unsigned long rssTotal, unsigned long pssTotal,
        unsigned long shared_clean_total, unsigned long private_dirty_total,
        unsigned long swap_pss_total)
    {
        if (!root) return;

        // Create a JSON object for process totals
        cJSON *totals = cJSON_CreateObject();
        if (!totals) {
            return;
        }

        // Add the total values
        cJSON_AddNumberToObject(totals, "rss_total", rssTotal);
        cJSON_AddNumberToObject(totals, "pss_total", pssTotal);
        cJSON_AddNumberToObject(totals, "shared_clean_total", shared_clean_total);
        cJSON_AddNumberToObject(totals, "private_dirty_total", private_dirty_total);
        cJSON_AddNumberToObject(totals, "swap_pss_total", swap_pss_total);

        // Add the totals object to the root with key "process_total"
        cJSON_AddItemToObject(root, "process_total", totals);
    }
#endif

/**
 * Inserts a new process info node into the linked list, sorted by descending
 * pssTotal.
 */
void addProcessInfo(Process_Info *addPInfo)
{
    Process_Info *addNode = (Process_Info *)malloc(sizeof(Process_Info));
    if (addNode)
    {
        Process_Info *tmp = headProcessInfo;
        Process_Info *prev = NULL;
        memcpy(addNode, addPInfo, sizeof(Process_Info));
        while (tmp)
        {
            if (tmp->pssTotal < addNode->pssTotal)
            {
                if (prev)
                {
                    prev->next = addNode;
                    addNode->next = tmp;
                }
                else
                {
                    headProcessInfo = addNode;
                    headProcessInfo->next = tmp;
                }
                return;
            }
            prev = tmp;
            tmp = tmp->next;
        }
        if (headProcessInfo)
        {
            prev->next = addNode;
        }
        else
        {
            headProcessInfo = addNode;
        }
    }
}

// -----------------------------
// Process Info Parsing
// -----------------------------

/**
 * Parses /proc/[pid]/smaps for a given process and accumulates memory
 * statistics. Returns 0 on success, 1 on failure.
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
    sprintf(tmp, "/proc/%u/smaps", pid);
    FILE *smap = fopen(tmp, "r");
    if (smap)
    {
        unsigned lines_To_skip = linesToSkipForRss;
        unsigned skipped = 1; // Tracks current skips
        unsigned expect_rss = 1;
        unsigned expect_pss = 0;
        unsigned expect_shared_clean = 0;
        unsigned expect_private_dirty = 0;
        unsigned expect_swap_pss = 0;
        unsigned prev_pss = 0;
        while (fgets(tmp, 127, smap))
        {
            /*00010000-00041000 r-xp 00000000 fc:00 5600 /usr/sbin/dropbearmulti
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
              00050000-00051000 r--p 00030000 fc:00 5600 /usr/sbin/dropbearmulti
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
            if (linesToSkipForRollover)
            { // Learnt the format
                if (++skipped > lines_To_skip)
                {
                    if (expect_rss)
                    {
                        // rss = 0;
                        if (sscanf(tmp, "Rss: %u kB", &rss))
                        {
                            if (rss)
                            {
                                //PRINT_DBG_SCANNED("Read Rss (%u) after %u/%u lines --> %s\r", rss, skipped, lines_To_skip, tmp);
                                getProcessInfo.rssTotal += rss;
                                //PRINT_DBG_SCANNED("rssTotal (%lu)\n", getProcessInfo.rssTotal);
                                lines_To_skip = linesToSkipForPss;
                                skipped = 1;
                                expect_pss = 1;
                                expect_rss = 0;
                                continue;
                            }
                            else
                            {
                                lines_To_skip = linesToSkipIfRss0;
                                skipped = 1;
                                continue;
                            }
                        }
                    }
                    else if (expect_pss)
                    {
                        // pss = 0;
                        if (sscanf(tmp, "Pss: %u kB", &pss))
                        {
                            //PRINT_DBG_SCANNED("Read Pss (%u) after %u/%u lines  --> %s\r", pss, skipped, lines_To_skip, tmp);
                            getProcessInfo.pssTotal += pss;
                            prev_pss = pss;
                            lines_To_skip = linesToSkipForSharedClean;
                            skipped = 1;
                            expect_shared_clean = 1;
                            expect_pss = 0;
                            continue;
                        }
                    }
                    else if (expect_shared_clean)
                    {
                        // shared_clean = 0;
                        if (sscanf(tmp, "Shared_Clean: %u kB", &shared_clean))
                        {
                            //PRINT_DBG_SCANNED("Read shared_clean (%u)  %u/%u lines --> %s\r", shared_clean, skipped, lines_To_skip, tmp);
                            getProcessInfo.shared_clean_total += shared_clean;

                            if (!prev_pss)
                            {
                                // No need to look at private dirty and swap pss
                                expect_rss = 1;

                                lines_To_skip = linesToSkipForSwapPss
                                                    ? linesToSkipForDirtyPrivate + linesToSkipForSwapPss +
                                                          linesToSkipForRollover - 2
                                                    : linesToSkipForDirtyPrivate + linesToSkipForRollover - 1;
                            }
                            else
                            {
                                expect_private_dirty = 1;
                                lines_To_skip = linesToSkipForDirtyPrivate;
                            }
                            expect_shared_clean = 0;
                            skipped = 1;
                            continue;
                        }
                    }
                    else if (expect_private_dirty)
                    {
                        // private_dirty = 0;
                        if (sscanf(tmp, "Private_Dirty: %u kB", &private_dirty))
                        {
                            expect_private_dirty = 0;
                            //PRINT_DBG_SCANNED("Read private_dirty (%u)  %u/%u lines --> %s\r", private_dirty, skipped, lines_To_skip, tmp);
                            getProcessInfo.private_dirty_total += private_dirty;
                            skipped = 1;
                            if (linesToSkipForSwapPss)
                            {
                                expect_swap_pss = 1;
                                lines_To_skip = linesToSkipForSwapPss;
                            }
                            else
                            {
                                expect_rss = 1;
                                lines_To_skip = linesToSkipForRollover;
                            }
                            continue;
                        }
                    }
                    else if (expect_swap_pss)
                    {
                        // swap_pss = 0;
                        if (sscanf(tmp, "SwapPss: %u kB", &swap_pss))
                        {
                            expect_swap_pss = 0;
                            expect_rss = 1;
                            //PRINT_DBG_SCANNED("Read swap_pss (%u)  %u/%u lines --> %s\r", swap_pss, skipped, lines_To_skip, tmp);
                            getProcessInfo.swap_pss_total += swap_pss;
                            lines_To_skip = linesToSkipForRollover;
                            skipped = 1;
                            continue;
                        }
                    }
                }
                else
                {
                    continue;
                }
            }
            else
            { // Learn here
                if (!linesToSkipForRss)
                {
                    if (sscanf(tmp, "Rss: %u kB", &rss))
                    {
                        getProcessInfo.rssTotal += rss;
                        linesToSkipForRss = linesSkippedForRss + 1;
                        //PRINT_DBG_INITIAL("After %u lines Read Rss (%u) in --> %s\r", linesToSkipForRss, rss, tmp);
                        continue;
                    }
                    else
                    {
                        linesSkippedForRss++;
                    }
                }
                else if (!linesToSkipForPss)
                {
                    if (sscanf(tmp, "Pss: %u kB", &pss))
                    {
                        getProcessInfo.pssTotal += pss;
                        linesToSkipForPss = linesSkippedForPss + 1;
                        //PRINT_DBG_INITIAL("After %u lines Read Pss (%u) in --> %s\r", linesToSkipForPss, pss, tmp);
                        continue;
                    }
                    else
                    {
                        linesSkippedForPss++;
                    }
                }
                else if (!linesToSkipForSharedClean)
                {
                    if (sscanf(tmp, "Shared_Clean: %u kB", &shared_clean))
                    {
                        getProcessInfo.shared_clean_total += shared_clean;
                        linesToSkipForSharedClean = linesSkippedForSharedClean + 1;
                        //PRINT_DBG_INITIAL("After %u lines Read Shared_Clean (%u) in --> %s\r", linesToSkipForSharedClean, shared_clean, tmp);
                        continue;
                    }
                    else
                    {
                        linesSkippedForSharedClean++;
                    }
                }
                else if (!linesToSkipForDirtyPrivate)
                {
                    if (sscanf(tmp, "Private_Dirty: %u kB", &private_dirty))
                    {
                        getProcessInfo.private_dirty_total += private_dirty;
                        linesToSkipForDirtyPrivate = linesSkippedForDirtyPrivate + 1;
                        //PRINT_DBG_INITIAL("After %u lines Read DirtyPrivate (%u) in --> %s\r", linesToSkipForDirtyPrivate, private_dirty, tmp);
                        continue;
                    }
                    else
                    {
                        linesSkippedForDirtyPrivate++;
                    }
                }
                else if (!linesToSkipForSwapPss)
                {
                    if (sscanf(tmp, "SwapPss: %u kB", &swap_pss))
                    {
                        getProcessInfo.swap_pss_total += swap_pss;
                        linesToSkipForSwapPss = linesSkippedForSwapPss + 1;
                        //PRINT_DBG_INITIAL("After %u lines Read Swap_Pss (%u) in --> %s\r", linesToSkipForSwapPss, swap_pss, tmp);
                        continue;
                    }
                    else
                    {
                        // check if smap doesn't contain swappss..
                        if (sscanf(tmp, "Rss: %u kB", &rss))
                        {
                            getProcessInfo.rssTotal += rss;
                            linesToSkipForRollover = linesSkippedForSwapPss + 1;
                            //PRINT_DBG_INITIAL("*****No SwapPss....skipping\n");
                            //PRINT_DBG_INITIAL("After %u lines Read Rss (%u) in --> %s\r", linesToSkipForRollover, rss, mp);
                            lines_To_skip = linesToSkipForPss;
                            expect_pss = 1;
                            expect_rss = 0;
                            linesToSkipIfRss0 = linesToSkipForPss + linesToSkipForSharedClean +
                                                linesToSkipForDirtyPrivate + linesToSkipForRollover - 3;
                            continue;
                        }
                        else
                        {
                            linesSkippedForSwapPss++;
                        }
                    }
                }
                else if (!linesToSkipForRollover)
                {
                    if (sscanf(tmp, "Rss: %u kB", &rss))
                    {
                        getProcessInfo.rssTotal += rss;
                        linesToSkipForRollover = linesSkippedForRollover + 1;
                        //PRINT_DBG_INITIAL("After %u lines Read Rss (%u) in --> %s\r", linesToSkipForRollover, rss, tmp);
                        lines_To_skip = linesToSkipForPss;
                        expect_pss = 1;
                        expect_rss = 0;
                        linesToSkipIfRss0 = linesToSkipForPss + linesToSkipForSharedClean + linesToSkipForDirtyPrivate +
                                            linesToSkipForSwapPss + linesToSkipForRollover - 4;
                        continue;
                    }
                    else
                    {
                        linesSkippedForRollover++;
                    }
                }
                else
                {
                    printf("%s:%d: Shouldn't get here..\n", __FUNCTION__, __LINE__);
                }
            }
        }
        fclose(smap);
        return 0;
    }
    else
    {
        printf("%s: Open failed, errno %d [%s]\n", tmp, errno, strerror(errno));
    }
    return 1;
}

/**
 * Prints detailed help and usage information and exits.
 */
void printHelpAndUsage(char *argv[], bool moreInfo)
{
    printf("MemInsight, a lightweight configurable Linux tool for collecting detailed system and per-process memory and CPU statistics");
    printf("Usage: %s [OPTIONS]\n", XMEMINSIGHT_BIN);
    printf("A lightweight, configurable tool for collecting detailed system and per-process memory and CPU statistics.\n\n");

    printf("Options:\n");
    printf("  -v, --version              %sversion info\n", XMEMINSIGHT_BIN);
    printf("  -a, --all                  Include kernel threads for process monitoring\n");
    printf("  -c, --config <file>        Path to configuration file with %s extension\n", CONFIG_EXTN);
    printf("  -o, --output <directory>   Output directory for generated CSV files (default: %s)\n", DEFAULT_OUT_DIR);
    printf("      --interval <seconds>   Interval in seconds between iterations (overrides config)\n");
    printf("      --iterations <count>   Number of iterations to run (overrides config)\n");
    printf("      --fmt <format>         Output format: csv (default) or json\n");
    printf("  -h, --help                 Show this help message and exit\n");
    printf("  -n, --numprocs <count>     Limit output to top N processes by PSS usage\n");
    printf("  -t, --test                 Run in test mode with a generated minimal config\n\n");

    if (moreInfo)
    {
        printf("Precedence:\n");
        printf("  Command-line arguments override config file values, which override built-in defaults.\n\n");

        printf("Default behavior (no flags):\n");
        printf("  - Runs indefinite number of iterations, with an interval of 15 minutes, monitors all processes with log level INFO\n");
        printf("  - Output: /tmp/<MAC>_<timestamp>_iter<iteration>_%s\n\n", CSV_FILE_NAME);

        printf("Example:\n");
        printf("  %s\n", argv[0]);
        printf("  %s --all\n", argv[0]);
        printf("  %s --config /etc/xmeminsight_configuration%s\n", argv[0], CONFIG_EXTN);
        printf("  %s -c myconfig%s -a --interval 10 --iterations 5\n", argv[0], CONFIG_EXTN);
        printf("  %s --output /var/log/ --iterations 3\n", argv[0]);
        printf("  %s --test\n\n", argv[0]);

        printf("Sample config file:\n");
        printf("  process_whitelist=myapp,systemd,1234\n  output_dir=/var/log/\n  iterations=10\n  interval=60\n  log_level=INFO\n\n");

        printf("Notes:\n");
        printf("  - Supported config file extensions: %s\n", CONFIG_EXTN);
        printf("  - If both interval and iterations are set via CLI, both are used.\n");
        printf("  - If only one is set, the other uses its default or config value.\n");
        printf("  - Output file name format: <MAC>_<TIMESTAMP>_iter<iteration>_%s\n", CSV_FILE_NAME);
    }
    exit(1);
}

/**
 * Collects system-wide memory statistics and writes to output file.
 * Returns 0 on success, -1 on failure.
 */
int collectSystemMemoryStats(bool includeKthreads, const char *outDir, int iterations, int interval, bool long_run, bool useJsonFormat)
{
    for (int iter = 0; long_run || iter < iterations; iter++)
    {
        printf("\n==== Iteration %d%s ====\n", iter + 1, long_run ? "/∞" : "");
        unsigned int noOfPids = 0;

        // MAC Address
        char mac[32] = {0};
        getMacAddress(deviceIdentifierName, mac, sizeof(mac));
        if (mac[0] == '\0')
        {
            strncpy(mac, DEFAULT_MAC, sizeof(mac) - 1);
            mac[sizeof(mac) - 1] = '\0';
        }

        // Current timestamp
        time_t timenow = time(NULL);
        struct tm *tm_info = localtime(&timenow);
        char timestamp[32] = {0};
        char ts[32] = {0};
        strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", tm_info);
        strftime(timestamp, sizeof(timestamp), "%Y%m%d%H%M%S", tm_info);

        // Firmware image name
        char fwName[FW_LEN] = {0};
        getFirmwareImageName(fwName, sizeof(fwName));

        // outDir or default /tmp/meminsight
        const char *dir = (outDir && outDir[0]) ? outDir : DEFAULT_OUT_DIR;
        ensure_output_dir(dir);
        char outputfile[512] = {0};

        const char* file_ext = useJsonFormat ? JSON_FILE_NAME : CSV_FILE_NAME;
        snprintf(outputfile, sizeof(outputfile), "%s/%s_%s_iter%d_%s", dir, mac, timestamp, iter + 1, file_ext);

        printf("Capturing System wide stats into %s\n", outputfile);
        FILE *output = fopen(outputfile, "w");
        if (NULL == output)
        {
            printf("%s: Open failed, %d [%s]\n", outputfile, errno, strerror(errno));
            return -1;
        }

        unsigned long rssTotal = 0, pssTotal = 0, shared_clean_total = 0, private_dirty_total = 0, swap_pss_total = 0;
        DIR *proc = opendir(PROC_DIR);
        if (proc)
        {
            struct dirent *entry;
            while ((entry = readdir(proc)) != NULL)
            {
                if (entry->d_name[0] != '.')
                {
                    unsigned pid = atoi(entry->d_name);
                    if (pid > 0)
                    {
                        unsigned flags = 0;
                        memset(&getProcessInfo, 0, sizeof(getProcessInfo));
                        if (!fillProcessStatFields(pid, &getProcessInfo, &flags))
                        {
                            printf("Failed to fill process stat fields for PID %d\n", pid);
                            continue;
                        }
                        if (flags & PF_KTHREAD && !includeKthreads)
                        {             /*PF_KTHREAD*/
                            continue; // Skip kernel threads if not included
                        }
                        if (!getProcessInfos(pid))
                        {
                            getProcessInfo.pid = pid;
                            // name, majFaults, cputime already filled by
                            // fillProcessStatFields
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
        }
        else
        {
            printf("Failed to open %s directory: %s\n", PROC_DIR, strerror(errno));
            fclose(output);
            return -1;
        }

        if (useJsonFormat)
        {
#ifdef ENABLE_CJSON
            // Create root JSON object
            cJSON *root = cJSON_CreateObject();
            if (root) {
                // Add Mem info
                addMemInfoJSON(root);

                // Add process info (this will also free the linked list)
                addProcessInfoJSON(root, noOfPids, output);

                // Add process totals
                addProcessTotalsJSON(root, rssTotal, pssTotal, shared_clean_total, private_dirty_total, swap_pss_total);

                char *json_string = cJSON_PrintUnformatted(root);
                if (json_string)
                {
                    fprintf(output, "%s\n", json_string);
                    free(json_string);
                }
                else
                {
                    printf("Failed to print JSON string\n");
                }
                // Clean up
                cJSON_Delete(root);
                // Ensure head is reset even if addProcessInfoJSON wasn't called
                // or didn't complete successfully
                headProcessInfo = NULL;
            }
#else
            printf("Warning: JSON format requested but cJSON support not enabled at build time.\n");
            printf("         Falling back to CSV format.\n");
            useJsonFormat = false;
#endif
        }
        else
        {
            fprintf(output, "FIRMWARE_NAME,MAC_ADDRESS,TIMESTAMP,REPORT_VERSION\n");
            fprintf(output, "%s,%s,%s,%s\n\n", fwName, mac, ts, reportVersion);
            saveMeminfo(output);
            printf("\nProcessed %u processes\n>> RSS_Total %lu\n>> PSS_Total "
               "%lu\n>> Shared_Clean_Total %lu\n>> Private_Dirty_Total %lu\n",
               noOfPids, rssTotal, pssTotal, shared_clean_total, private_dirty_total);
            fprintf(output, "\n\nProcesses:\nPID,EXE,RSS,PSS,SHARED_CLEAN,PRIVATE_"
                        "DIRTY,SWAP_PSS,MAJ_FAULTS,CPU_TIME\n");
            writeProcessInfo(noOfPids, output);
            // Redundant but added for safety - writeProcessInfo already resets headProcessInfo
            // This ensures it's reset even if writeProcessInfo didn't run completely
            headProcessInfo = NULL;
            fprintf(output, "0,Total,%lu,%lu,%lu,%lu,%lu,0,0,0\n", rssTotal, pssTotal, shared_clean_total, private_dirty_total, swap_pss_total);
        }
        fclose(output);

        if (interval >= 0 && (long_run || iter + 1 < iterations))
        {
            printf("* %d%s Iteration completed. Sleeping for %d seconds before next iteration... \n", iter + 1, long_run ? "/∞" : "", interval);
            sleep(interval);
        }
    }
    printf("\n---- Completed Data Capture ----\n");
    return 0;
}

int handleConfigMode(const char *confFile, const char *cli_out_dir, int cli_iterations, int cli_interval, bool enableKThreads, bool long_run, bool useJsonFormat)
{
    Config_Data config;
    if (parseConfig(confFile, &config) != 0)
    {
        printf("Error: Failed to parse config file '%s'\n", confFile);
        return -1; // TODO: Need to handle this better
    }

    // Output directory: CLI > config > default
    const char *final_out_dir = (cli_out_dir[0] && strncmp(cli_out_dir, DEFAULT_OUT_DIR, strlen(DEFAULT_OUT_DIR) + 1) != 0)
                                    ? cli_out_dir
                                    : (config.outputDir[0] ? config.outputDir : DEFAULT_OUT_DIR);

    // Interval/Iterations logic (CLI > config > default)
    int final_iterations = config.iterations;
    int final_interval = config.interval;

    if (cli_iterations > 0 && cli_interval > 0)
    { // valid CLI values
        final_iterations = cli_iterations;
        final_interval = cli_interval;
    }
    else if (cli_iterations > 0 && (cli_interval <= 0))
    { // only iterations is valid
        final_iterations = cli_iterations;
        final_interval = DEFAULT_INTERVAL;
    }
    else if ((cli_iterations <= 0) && cli_interval > 0)
    { // only interval is valid
        final_iterations = 2;
        final_interval = cli_interval;
    }
    else if ((cli_iterations <= 0) && (cli_interval <= 0))
    { // no valid CLI values
        // Use config values if present, else defaults
        if (final_iterations <= 0)
            final_iterations = DEFAULT_ITERATIONS;
        if (final_interval <= 0)
            final_interval = DEFAULT_INTERVAL;
    }

    // Defensive: if config values are negative or zero, use defaults
    if (final_iterations <= 0)
        final_iterations = DEFAULT_ITERATIONS;
    if (final_interval < 0)
        final_interval = DEFAULT_INTERVAL;

    config.iterations = final_iterations;
    config.interval = final_interval;

    printf("* Config loaded successfully [%s]", confFile);
    //printf("\n whitelist=%p\n count=%u\n outDir=%s\n iterations=%u\n interval=%u\n logLevel=%s\n", config.whitelist, config.whiteListCount, config.outputDir, final_iterations, final_interval, config.logLevel);

    if (config.interval > 0 && config.iterations > 0)
    {
        long_run = false; // If both interval and iterations are specified, it's not a long run
    }

    for (int iter = 0; long_run || iter < final_iterations; iter++)
    {
        printf("\n==== Iteration %d%s ====\n", iter + 1, long_run ? "/∞" : "");
        // Get MAC address
        char mac[32] = {0};
        getMacAddress(deviceIdentifierName, mac, sizeof(mac));
        if (mac[0] == '\0') {
            strncpy(mac, DEFAULT_MAC, sizeof(mac) - 1);
            mac[sizeof(mac) - 1] = '\0'; // Ensure null-termination
        }

        // Get timestamp
        time_t timenow = time(NULL);
        struct tm *tm_info = localtime(&timenow);
        char timestamp[32] = {0};
        char ts[32] = {0};
        strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", tm_info);
        strftime(timestamp, sizeof(timestamp), "%Y%m%d%H%M%S", tm_info);

        // Firmware image name
        char fwName[FW_LEN] = {0};
        getFirmwareImageName(fwName, sizeof(fwName));

        // Generate output file name
        char outputFilePath[PATH_MAX * 2] = {0};
        ensure_output_dir(final_out_dir);

        const char* file_ext = useJsonFormat ? JSON_FILE_NAME : CSV_FILE_NAME;
        snprintf(outputFilePath, sizeof(outputFilePath), "%s/%s_%s_iter%d_meminsight.%s", final_out_dir, mac, timestamp, iter+1, file_ext);

        printf("Capturing Config based  stats into %s\n", outputFilePath);
        // Open output file
        FILE *output = fopen(outputFilePath, "w");
        if (!output)
        {
            printf("Error: Failed to open output file '%s' for writing\n", outputFilePath);
            for (unsigned j = 0; j < config.whiteListCount; j++)
            {
                if (config.whitelist[j] != NULL)
                {
                    free(config.whitelist[j]);
                }
            }
            if (config.whitelist != NULL)
            {
                free(config.whitelist);
            }
            return -1;
        }

        unsigned long rssTotal = 0, pssTotal = 0, shared_clean_total = 0, private_dirty_total = 0, swap_pss_total = 0;
        unsigned actualCount = 0;
        if (config.whiteListCount > 0)
        {
            for (unsigned i = 0; i < config.whiteListCount; i++)
            {
                unsigned int pid = 0;
                printf("Processing whitelist item %s\n", config.whitelist[i]);
                if (isPID(config.whitelist[i]))
                {
                    pid = (unsigned int)atoi(config.whitelist[i]);
                }
                else
                {
                    if (!getPIDByProcessName(config.whitelist[i], &pid))
                    {
                        printf("Process name'%s' not found\n", config.whitelist[i]);
                        continue; // Skip to next item
                    }
                }
                memset(&getProcessInfo, 0, sizeof(getProcessInfo));
                unsigned flags = 0;
                if (!fillProcessStatFields(pid, &getProcessInfo, &flags))
                {
                    printf("Failed to fill process stat fields for PID %u\n", pid);
                    continue; // Skip to next item
                }
                if (flags & PF_KTHREAD && !enableKThreads)
                {             /*PF_KTHREAD*/
                    continue; // Skip kernel threads if not included
                }
                if (!getProcessInfos(pid))
                {
                    getProcessInfo.pid = pid;
                    // name, majFaults, cputime already filled by
                    // fillProcessStatFields
                    rssTotal += getProcessInfo.rssTotal;
                    pssTotal += getProcessInfo.pssTotal;
                    shared_clean_total += getProcessInfo.shared_clean_total;
                    private_dirty_total += getProcessInfo.private_dirty_total;
                    swap_pss_total += getProcessInfo.swap_pss_total;
                    addProcessInfo(&getProcessInfo);
                    actualCount++;
                }
                else
                {
                    printf("Failed to get process info for PID %u\n", pid);
                }
            }

            if (useJsonFormat)
            {
#ifdef ENABLE_CJSON
                cJSON *root = cJSON_CreateObject();
                if (root) {
                    // Add Mem info
                    addMemInfoJSON(root);

                    // Add process info (this will also free the linked list)
                    addProcessInfoJSON(root, actualCount, output);

                    // Add process totals
                    addProcessTotalsJSON(root, rssTotal, pssTotal, shared_clean_total, private_dirty_total, swap_pss_total);

                    char *json_string = cJSON_PrintUnformatted(root);
                    if (json_string)
                    {
                        fprintf(output, "%s\n", json_string);
                        free(json_string);
                    }
                    else
                    {
                        printf("Failed to print JSON string\n");
                    }
                    // Clean up
                    cJSON_Delete(root);
                    // Ensure head is reset even if addProcessInfoJSON wasn't called
                    // or didn't complete successfully
                    headProcessInfo = NULL;
                }
#else
                printf("Warning: JSON format requested but cJSON support not enabled at build time.\n");
                printf("         Falling back to CSV format.\n");
                useJsonFormat = false;
#endif
            }
            else
            {
                fprintf(output, "FIRMWARE_NAME,MAC_ADDRESS,TIMESTAMP,REPORT_VERSION\n");
                fprintf(output, "%s,%s,%s,%s\n\n", fwName, mac, ts, reportVersion);
                saveMeminfo(output); // TODO: based on whitelist count write content
                fprintf(output, "\nPID,EXE,RSS,PSS,SHARED_CLEAN,PRIVATE_DIRTY,SWAP_PSS,"
                    "MAJ_FAULTS,CPU_TIME\n"); // TODO: bring inside loop
                writeProcessInfo(actualCount, output);
                fprintf(output, "0,Total,%lu,%lu,%lu,%lu\n", rssTotal, pssTotal, shared_clean_total, private_dirty_total);
            }
            headProcessInfo = NULL; // Reset for next iteration
        }
        else
        {
            printf("No whitelist specified in config. Collecting data for all processes\n");
            unsigned int noOfPids = 0;

            DIR *proc = opendir(PROC_DIR);
            if (proc)
            {
                struct dirent *entry;
                while ((entry = readdir(proc)) != NULL)
                {
                    if (entry->d_name[0] != '.')
                    {
                        unsigned pid = atoi(entry->d_name);
                        if (pid > 0)
                        {
                            unsigned flags = 0;
                            memset(&getProcessInfo, 0, sizeof(getProcessInfo));
                            if (!fillProcessStatFields(pid, &getProcessInfo, &flags))
                            {
                                printf("Failed to fill process stat fields for PID %d\n", pid);
                                continue;
                            }
                            if (flags & PF_KTHREAD && !enableKThreads)
                            {             /*PF_KTHREAD*/
                                continue; // Skip kernel threads if not included
                            }
                            if (!getProcessInfos(pid))
                            {
                                getProcessInfo.pid = pid;
                                // name, majFaults, cputime already filled by
                                // fillProcessStatFields
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
                if (useJsonFormat)
                {
#ifdef ENABLE_CJSON
                    cJSON *root = cJSON_CreateObject();
                    if (root) {
                        // Add Mem info
                        addMemInfoJSON(root);

                        // Add process info (this will also free the linked list)
                        addProcessInfoJSON(root, noOfPids, output);

                        // Add process totals
                        addProcessTotalsJSON(root, rssTotal, pssTotal, shared_clean_total, private_dirty_total, swap_pss_total);

                        char *json_string = cJSON_PrintUnformatted(root);
                        if (json_string)
                        {
                            fprintf(output, "%s\n", json_string);
                            free(json_string);
                        }
                        else
                        {
                            printf("Failed to print JSON string\n");
                        }
                        // Clean up
                        cJSON_Delete(root);
                        // Ensure head is reset even if addProcessInfoJSON wasn't called
                        // or didn't complete successfully
                        headProcessInfo = NULL;
                    }
#else
                    printf("Warning: JSON format requested but cJSON support not enabled at build time.\n");
                    printf("         Falling back to CSV format.\n");
                    useJsonFormat = false;
#endif
                }
                else
                {
                    fprintf(output, "FIRMWARE_NAME,MAC_ADDRESS,TIMESTAMP,REPORT_VERSION\n");
                    fprintf(output, "%s,%s,%s,%s\n\n", fwName, mac, ts, reportVersion);
                    saveMeminfo(output);
                    fprintf(output, "\n\nProcesses:\nPID,EXE,RSS,PSS,SHARED_CLEAN,PRIVATE_DIRTY,SWAP_PSS,MAJ_FAULTS,CPU_TIME\n");
                    writeProcessInfo(noOfPids, output);
                    // Redundant but added for safety - writeProcessInfo already resets headProcessInfo
                    // This ensures it's reset even if writeProcessInfo didn't complete successfully
                    headProcessInfo = NULL;
                    fprintf(output, "0,Total,%lu,%lu,%lu,%lu,%lu,0,0,0\n", rssTotal, pssTotal, shared_clean_total, private_dirty_total, swap_pss_total);
                }
            }
            else
            {
                printf("Failed to open %s directory: %s\n", PROC_DIR, strerror(errno));
                fclose(output);
                return -1;
            }
        }
        fclose(output);

        if (final_interval > 0 && iter + 1 < final_iterations)
        {
            printf("* %d/%d Completed. Sleeping for %d seconds before next iteration... \n", iter+1, final_iterations, final_interval);
            sleep(final_interval);
        }
    }

    // Free the whitelist memory
    for (unsigned j = 0; j < config.whiteListCount; j++)
    {
        if (config.whitelist[j] != NULL)
        {
            free(config.whitelist[j]);
        }
    }
    if (config.whitelist != NULL)
    {
        free(config.whitelist);
    }
    printf("\n---- Completed Data Capture ----\n");
    return 0;
}

/**
 * Reads /proc/meminfo and writes the first 50 fields and their values to the
 * output file.
 */
void saveMeminfo(FILE *out)
{
    unsigned long val[50];
    unsigned count = 0;
    char tmp[128];
    char name[64];

    FILE *meminfo = fopen("/proc/meminfo", "r");
    if (meminfo)
    {
        int first = 1;
        // print header
        while (fgets(tmp, 127, meminfo) && count < 50)
        {
            if (sscanf(tmp, "%s %lu kB", name, &val[count]) == 2)
            {
                name[strlen(name) - 1] = '\0'; // Remove trailing ':'
                if (!first)
                {
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
        while (fgets(tmp, 127, meminfo) && count < 50)
        {
            if (sscanf(tmp, "%s %lu kB", name, &val[count]) == 2)
            {
                if (!first)
                {
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

#ifdef ENABLE_CJSON
/**
 * Adds memory information to the root JSON object
 */
void addMemInfoJSON(cJSON *root)
{
    if (!root) return;
    char tmp[128];
    char name[64];
    unsigned long value;

    cJSON *memInfo = cJSON_CreateObject();
    if (!memInfo) {
        return;
    }

    FILE *meminfo_file = fopen("/proc/meminfo", "r");
    if (meminfo_file) {
        while (fgets(tmp, sizeof(tmp) - 1, meminfo_file)) {
            if (sscanf(tmp, "%s %lu kB", name, &value) == 2) {
                name[strlen(name) - 1] = '\0'; // Remove trailing ':'
                cJSON_AddNumberToObject(memInfo, name, value);
            }
        }
        fclose(meminfo_file);
        cJSON_AddItemToObject(root, "memory_info", memInfo);
    } else {
        // Failed to open meminfo
        cJSON_Delete(memInfo);
    }
}
#endif

// -----------------------------
// Main Program
// -----------------------------

/**
 * Main entry point: parses arguments, scans processes, collects stats, writes
 * output.
 */
int main(int argc, char *argv[])
{
    // set defaults
    bool isConfigPresent = false;
    bool enableKThreads = false;
    bool isTestMode = false;
    bool isSystemWide = true;
    bool long_run = true;
    char confFile[PATH_MAX] = {0};
    char out_dir[PATH_MAX] = DEFAULT_OUT_DIR;
    int cli_iterations = -1;
    int cli_interval = -1;
    bool useJsonFormat = false;

    // CLI parsing and initialization
    if (argc == 1)
    {
        isSystemWide = true;
    }

    for (int i = 1; i < argc; i++)
    {
        if (!strncmp(argv[i], "-c", 3) || !strncmp(argv[i], "--config", 9))
        { // config file
            if (i + 1 < argc)
            {
                strncpy(confFile, argv[i + 1], PATH_MAX - 1);
                FILE *fp = fopen(confFile, "r");
                if (!fp)
                {
                    printf("Error: Config file '%s' does not exist or cannot "
                           "be opened.\n",
                           confFile);
                    printHelpAndUsage(argv, true);
                }
                fclose(fp);
                isSystemWide = false; // Config mode implies not system-wide
                isConfigPresent = true;
                i++; // Skip next arg (conf file)
            }
            else
            {
                printf("Error: Missing config file path after %s\n", argv[i]);
                printHelpAndUsage(argv, false);
            }
        }
        else if (!strncmp(argv[i], "-a", 3) || !strncmp(argv[i], "--all", 6))
        { // include kernel threads
            enableKThreads = true;
        }
        else if (!strncmp(argv[i], "-h", 3) || !strncmp(argv[i], "--help", 7))
        { // help
            printHelpAndUsage(argv, true);
        }
        else if (!strncmp(argv[i], "-t", 3) || !strncmp(argv[i], "--test", 7))
        { // test mode
            isTestMode = true;
        }
        else if (!strncmp(argv[i], "-o", 3) || !strncmp(argv[i], "--output", 9))
        { // output directory
            if (i + 1 < argc)
            {
                strncpy(out_dir, argv[i + 1], PATH_MAX - 1);
                i++; // skip next arg (output directory)
            }
            else
            {
                printf("Error: Missing output directory path after %s\n", argv[i]);
                printHelpAndUsage(argv, false);
            }
        }
        else if (!strncmp(argv[i], "--interval", 11))
        { // interval
            if (i + 1 < argc)
            {
                cli_interval = atoi(argv[i + 1]);
                i++; // skip next arg (interval)
                long_run = false; // If interval is specified, it's not a long run
            }
            else
            {
                printf("Error: Missing interval value after %s\n", argv[i]);
                printHelpAndUsage(argv, false);
            }
        }
        else if (!strncmp(argv[i], "--iterations", 13))
        { // iterations
            if (i + 1 < argc)
            {
                cli_iterations = atoi(argv[i + 1]);
                i++; // skip next arg (iterations)
                long_run = false; // If iterations are specified, it's not a long run
            }
            else
            {
                printf("Error: Missing iterations value after %s\n", argv[i]);
                printHelpAndUsage(argv, false);
            }
        }
        else if (!strncmp(argv[i], "-n", 3) || !strncmp(argv[i], "--numprocs", 10))
        {
            if (i + 1 < argc)
            {
                g_processLimit = atoi(argv[i + 1]);
                if (g_processLimit <= 0) {
                    printf("Warning: Invalid process count '%s', using unlimited\n", argv[i+1]);
                    g_processLimit = -1;
                } else {
                    printf("* Limiting output to top %d processes\n", g_processLimit);
                }
                i++; // skip next arg (process count)
            }
            else
            {
                printf("Error: Missing process count value after %s\n", argv[i]);
                printHelpAndUsage(argv, false);
            }
        }
        else if (!strncmp(argv[i], "--fmt", 6))
        {
            if (i + 1 < argc)
            {
                if (!strncmp(argv[i+1], "json", 5))
                {
#ifdef ENABLE_CJSON
                    g_outputFormat = FORMAT_JSON;
                    useJsonFormat = true;
#else
                    printf("Warning: JSON format requested but cJSON support not enabled at build time.\n");
                    printf("         Falling back to CSV format.\n");
                    g_outputFormat = FORMAT_CSV;
#endif
                }
                else if (!strncmp(argv[i+1], "csv", 4))
                {
                    g_outputFormat = FORMAT_CSV;
                }
                else
                {
                    printf("Error: Unsupported format '%s'. Supported formats: csv, json\n", argv[i+1]);
                    printHelpAndUsage(argv, false);
                }
                i++; // skip the format parameter
            }
            else
            {
                printf("Error: Missing format value after --fmt\n");
                printHelpAndUsage(argv, false);
                return 1;
            }
        }
        else if (!strncmp(argv[i], "-v", 3) || !strncmp(argv[i], "--version", 10))
        {
            printf("%s v%s (Report v%s)\n", XMEMINSIGHT_BIN, xMemInsightVersion, reportVersion);
        }
        else
        {
            printf("Error: Unrecognized argument '%s'\n", argv[i]);
            printHelpAndUsage(argv, false);
        }
    }

    printf("\nExecuting: ");
    for (int i = 0; i < argc; i++)
    {
        printf("%s ", argv[i]);
    }
    printf("\n");

    includeKthreads = enableKThreads; // Cascade to global for all code paths
    if (isConfigPresent)
    {
        handleConfigMode(confFile, out_dir, cli_iterations, cli_interval, enableKThreads, long_run, useJsonFormat);
    }
    else if (isSystemWide)
    {
        int final_interval = 0;
        int final_iterations = 1;
        if (long_run) {
            final_interval = LONG_RUN_INTERVAL;
        }
        else {
            final_interval = (cli_interval <= 0) ? DEFAULT_INTERVAL : cli_interval;
            final_iterations = (cli_iterations <= 1) ? ((final_interval > 0) ? 2 : DEFAULT_ITERATIONS) : cli_iterations;
        }
        printf("* Running %d iterations with %ds interval (indefinitely?: %s)\n", final_iterations, final_interval, long_run ? "yes" : "no");
        collectSystemMemoryStats(enableKThreads, out_dir, final_iterations, final_interval, long_run, useJsonFormat);
    }
    else if (isTestMode)
    {
        printf("Running in test mode with minimal config...\n");
        printf("TO BE IMPLEMENTED\n"); // TODO: Implement test mode logic
    }
}
