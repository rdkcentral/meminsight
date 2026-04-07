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

#include "config.h"
#include "memstatus.h"

#ifdef ENABLE_CJSON
#include <dlfcn.h>

/*
 * Function pointer table for all cJSON symbols we need.
 * Resolved at runtime by loadCjson(); zeroed on unload.
 */
typedef struct {
    void    *handle;
    cJSON_t *(*CreateObject)(void);
    cJSON_t *(*CreateArray)(void);
    cJSON_t *(*AddStringToObject)(cJSON_t *obj, const char *name, const char *str);
    cJSON_t *(*AddNumberToObject)(cJSON_t *obj, const char *name, double n);
    cJSON_t *(*AddItemToObject)(cJSON_t *obj, const char *key, cJSON_t *item);
    cJSON_t *(*AddItemToArray)(cJSON_t *arr, cJSON_t *item);
    char    *(*Print)(const cJSON_t *item);
    char    *(*PrintUnformatted)(const cJSON_t *item);
    void     (*Delete)(cJSON_t *item);
} CjsonLib;

static CjsonLib g_cjson;
static cJSON_t *g_rootObject = NULL;

/*
 * -----------------------------------------------------------------------
 * WHY DLOPEN INSTEAD OF STATIC LINKING
 * -----------------------------------------------------------------------
 * cJSON is an optional feature. We load it at runtime via dlopen() rather
 * than linking -lcjson at build time for two key reasons:
 *
 *  1. Portability: the xmeminsight binary deploys on embedded/RDK targets
 *     that may not have libcjson installed. On those devices CSV output
 *     continues to work as-is — no library, no problem.
 *
 *  2. Zero overhead when unused: if --fmt json is never passed, dlopen()
 *     is never called. The JSON code path stays completely dormant — no
 *     library mapped into memory, no symbols resolved.
 *
 * On load failure we log a clear diagnostic and fall back to CSV so the
 * capture session continues rather than aborting. For embedded targets a
 * data gap is almost always better than a dead process.
 *
 * If JSON output ever becomes a hard requirement for a deployment, replace
 * this block with a static -lcjson link and remove the CSV fallback below.
 * -----------------------------------------------------------------------
 */
static int loadCjson(void)
{
    memset(&g_cjson, 0, sizeof(g_cjson));

    /* Try the versioned SO first; fall back to the bare soname. */
    g_cjson.handle = dlopen("libcjson.so.1", RTLD_LAZY | RTLD_LOCAL);
    if (!g_cjson.handle)
        g_cjson.handle = dlopen("libcjson.so", RTLD_LAZY | RTLD_LOCAL);

    if (!g_cjson.handle) {
        PRINT_MUST("JSON: Failed to load libcjson: %s\n", dlerror());
        PRINT_MUST("JSON: Falling back to CSV format.\n");
        g_reportFormat = REPORT_CSV;
        return -1;
    }

#define LOAD_SYM(fn) \
    do { \
        g_cjson.fn = (void *)dlsym(g_cjson.handle, "cJSON_" #fn); \
        if (!g_cjson.fn) { \
            PRINT_MUST("JSON: Failed to resolve cJSON_" #fn ": %s\n", dlerror()); \
            dlclose(g_cjson.handle); \
            memset(&g_cjson, 0, sizeof(g_cjson)); \
            PRINT_MUST("JSON: Symbol resolution failed. Falling back to CSV.\n"); \
            g_reportFormat = REPORT_CSV; \
            return -1; \
        } \
    } while (0)

    LOAD_SYM(CreateObject);
    LOAD_SYM(CreateArray);
    LOAD_SYM(AddStringToObject);
    LOAD_SYM(AddNumberToObject);
    LOAD_SYM(AddItemToObject);
    LOAD_SYM(AddItemToArray);
    LOAD_SYM(Print);
    LOAD_SYM(PrintUnformatted);
    LOAD_SYM(Delete);

#undef LOAD_SYM

    PRINT_INFO("JSON: libcjson loaded successfully.\n");
    return 0;
}

static void unloadCjson(void)
{
    if (g_cjson.handle) {
        dlclose(g_cjson.handle);
    }
    memset(&g_cjson, 0, sizeof(g_cjson));
}
#endif /* ENABLE_CJSON */

// -----------------------------
// Global Variables
// -----------------------------
Process_Info getProcessInfo = {0};    // Temporary struct for collecting process info
Process_Info *headProcessInfo = NULL; // Head of linked list
Report_Format g_reportFormat  = REPORT_CSV; // Active output format (default: REPORT_CSV)
bool g_jsonPrettyPrint         = false;
bool g_bwDataAvailable = false;
unsigned smaps_rollup;                // Effective source for current parse: 1=smaps_rollup, 0=smaps
unsigned force_smaps = 0;             // CLI override: force reading /proc/<pid>/smaps
char gSMAPS_OR_ROLLUP[] = "smaps_rollup";

#ifdef TESTME
unsigned isTestMode = 0;
char testSmap[128];
char testMeminfo[128];
char testBuddyinfo[128];
char testPagetypeinfo[128];
Process_Info processInfoTest;
static int unitTestFailed = 0;
#endif

static const char deviceIdentifierName[] = DEVICE_IDENTIFIER;
static const char memInsightVersion[] = "" MEMINSIGHT_MAJOR_VERSION "." MEMINSIGHT_MINOR_VERSION "";
static const char reportVersion[] =  "" REPORT_MAJOR_VERSION "." REPORT_MINOR_VERSION "";

int (*getProcessInfos_ptr)(FILE*);

// -----------------------------
// Utility Functions
// -----------------------------

/**
 * Ensures that the specified output directory exists.
 * If it doesn't exist, it attempts to create it.
 * Returns true if directory exists or was successfully created.
 */
static bool ensure_output_dir(const char *dir)
{
    struct stat st = {0};
    if (stat(dir, &st) == 0) // Exists
    {
        if (S_ISDIR(st.st_mode)) // Is a directory
        {
            return true;
        }
        PRINT_MUST("Path '%s' exists but is not a directory\n", dir);
        return false;
    }
    if (mkdir(dir, 0755) == -1) // Try to create
    {
        PRINT_MUST("Failed to create output directory '%s': %s\n", dir, strerror(errno));
        return false;
    }
    return true;
}

/**
 * @brief Initialize runtime setup information (one-time cache).
 *
 * Populates a SetupInfo struct once at startup:
 *  - Output directory, MAC address, firmware name (cached for entire run)
 *  - Report filename based on format (cached)
 *  - Kernel version (captured ONCE, reused across all iterations)
 *
 * NOTE: uptime and timestamp are NOT cached here — they are calculated
 * fresh on each report iteration in collectSystemMemoryStats() to reflect
 * current system state at report generation time.
 *
 * @param[in] outDir   Preferred output directory (may be NULL or empty).
 * @param[in] format   Report format (REPORT_CSV or REPORT_JSON).
 * @return SetupInfo populated structure (by-value).
 */
SetupInfo initializeSetupInfo(const char *outDir, Report_Format format)
{
    SetupInfo info = {0};

    /* One-time directory and file setup. */
    info.outputDir = (outDir && *outDir) ? outDir : DEFAULT_OUT_DIR;
    info.dirCreated = ensure_output_dir(info.outputDir);
    info.reportFileName = (format == REPORT_JSON) ? JSON_FILE_NAME : CSV_FILE_NAME;

    /* One-time device metadata. */
    getMacAddress(deviceIdentifierName, info.mac, sizeof(info.mac));
    if (info.mac[0] == '\0') {
        strncpy(info.mac, DEFAULT_MAC, sizeof(info.mac) - 1);
        info.mac[sizeof(info.mac) - 1] = '\0';
    }
    getFirmwareImageName(info.fwName, sizeof(info.fwName));

    /* Cache kernel version once (stable across iterations). */
    const char *kv = getKernelVersion();
    strncpy(info.kernelVersion, kv, sizeof(info.kernelVersion) - 1);
    info.kernelVersion[sizeof(info.kernelVersion) - 1] = '\0';

    return info;
}

static void updateBandwidthAvailability(void)
{
    ((access(BW_DDR_MODE_FILE, F_OK) == 0) && (access(BW_DDR_FILE, F_OK) == 0)) ? (g_bwDataAvailable = true) : (g_bwDataAvailable = false);
}

/**
 * Reads a property value from a file in "key=value" format.
 * Returns 1 if found, 0 otherwise.
 */
#if 0
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
#endif

/**
 * Retrieves the system uptime from /proc/uptime.
 *
 * /proc/uptime format:  <uptime_seconds> <idle_seconds>
 *   uptime_seconds  - total wall-clock seconds since boot (fractional).
 *   idle_seconds    - sum of per-CPU idle time (fractional, not used here).
 *
 * Example:  "10658.36 37479.52"
 *   10658 s  →  2 h  57 m  38 s  (matches `uptime` output: "up  2:57")
 *
 * double is used for uptime_seconds so that precision is preserved for
 * devices running longer than ~194 days (where float would lose sub-second
 * accuracy and eventually whole seconds).
 *
 * Returns a pointer to a static buffer in "DDd HH:MM:SS" format, or
 * "unknown" if /proc/uptime cannot be read.
 */
const char *getSystemUptime(void)
{
    static char uptimeStr[32] = {0};
    FILE *fp = fopen(UPTIME_FILE, "r");
    if (!fp)
    {
        strncpy(uptimeStr, "unknown", sizeof(uptimeStr) - 1);
        uptimeStr[sizeof(uptimeStr) - 1] = '\0';
        return uptimeStr;
    }

    double uptimeSeconds = 0.0;
    int parsed = fscanf(fp, "%lf", &uptimeSeconds);
    fclose(fp);

    if (parsed != 1 || uptimeSeconds < 0.0)
    {
        strncpy(uptimeStr, "unknown", sizeof(uptimeStr) - 1);
        uptimeStr[sizeof(uptimeStr) - 1] = '\0';
        return uptimeStr;
    }

    unsigned long totalSec = (unsigned long)uptimeSeconds;
    unsigned int days    = (unsigned int)(totalSec / 86400);
    unsigned int hours   = (unsigned int)((totalSec % 86400) / 3600);
    unsigned int minutes = (unsigned int)((totalSec % 3600) / 60);
    unsigned int seconds = (unsigned int)(totalSec % 60);

    if (days > 0)
        snprintf(uptimeStr, sizeof(uptimeStr), "%ud %02u:%02u:%02u", days, hours, minutes, seconds);
    else
        snprintf(uptimeStr, sizeof(uptimeStr), "%02u:%02u:%02u", hours, minutes, seconds);

    return uptimeStr;
}

/**
 * Returns the running kernel version string.
 *
 * Uses uname(2) — a single syscall — which is lighter and more portable
 * than parsing /proc/version.  The release field (e.g. "5.15.0-91-generic"
 * or "4.14.71") is the standard string reported by `uname -r`.
 *
 * Returns a pointer to a static buffer, or "unknown" on failure.
 */
const char *getKernelVersion(void)
{
    static char kernelVer[64] = {0};
    struct utsname uts;
    if (uname(&uts) == 0)
    {
        strncpy(kernelVer, uts.release, sizeof(kernelVer) - 1);
        kernelVer[sizeof(kernelVer) - 1] = '\0';
    }
    else
    {
        strncpy(kernelVer, "unknown", sizeof(kernelVer) - 1);
        kernelVer[sizeof(kernelVer) - 1] = '\0';
    }
    return kernelVer;
}

static void trimTrailingWhitespace(char *str)
{
    size_t len = strlen(str);
    while (len > 0 && (str[len - 1] == '\n' || str[len - 1] == '\r' || str[len - 1] == ' ' || str[len - 1] == '\t')) {
        str[len - 1] = '\0';
        len--;
    }
}

static void trimLeadingWhitespace(char **str)
{
    while (**str == ' ' || **str == '\t') {
        (*str)++;
    }
}

static int parseUnsignedSeries(const char *start, unsigned long *values, int maxValues)
{
    int count = 0;
    const char *p = start;
    while (*p && count < maxValues) {
        while (*p == ' ' || *p == '\t')
            p++;
        if (*p == '\0' || *p == '\n' || *p == '\r')
            break;

        char *end = NULL;
        unsigned long v = strtoul(p, &end, 10);
        if (end == p)
            break;
        values[count++] = v;
        p = end;
    }
    return count;
}

static int writePagetypeInfoCSV(FILE *out)
{
#define FRAG_MAX_ORDERS 32
    FILE *fp = NULL;
#ifdef TESTME
    if (isTestMode) {
        if (!testPagetypeinfo[0])
            return -1;
        fp = fopen(testPagetypeinfo, "r");
    } else {
        fp = fopen(PGT_FILE, "r");
    }
#else
    fp = fopen(PGT_FILE, "r");
#endif
    if (!fp)
        return -1;

    char line[1024];
    int pageBlockOrder = -1;
    int pagesPerBlock = -1;
    int rowCount = 0;
    int headerCols = 0;
    bool headerWritten = false;

    fprintf(out, "\n\nFragmentation_PagetypeInfo:\n");
    fprintf(out, "STAT,VALUE\n");

    while (fgets(line, sizeof(line), fp)) {
        if (pageBlockOrder < 0 && sscanf(line, "Page block order: %d", &pageBlockOrder) == 1)
            continue;
        if (pagesPerBlock < 0 && sscanf(line, "Pages per block: %d", &pagesPerBlock) == 1)
            continue;

        unsigned node = 0;
        char zone[64] = {0};
        char type[64] = {0};
        int consumed = 0;
        int matched = sscanf(line, "Node %u, zone %63[^,], type %63s %n", &node, zone, type, &consumed);
        if (matched != 3 || consumed <= 0)
            continue;

        char *zonePtr = zone;
        trimLeadingWhitespace(&zonePtr);
        trimTrailingWhitespace(zonePtr);

        unsigned long values[FRAG_MAX_ORDERS] = {0};
        int valueCount = parseUnsignedSeries(line + consumed, values, FRAG_MAX_ORDERS);
        if (valueCount <= 0)
            continue;

        if (!headerWritten) {
            if (pageBlockOrder >= 0)
                fprintf(out, "page_block_order,%d\n", pageBlockOrder);
            if (pagesPerBlock >= 0)
                fprintf(out, "pages_per_block,%d\n", pagesPerBlock);

            fprintf(out, "\nNODE,ZONE,TYPE");
            headerCols = valueCount;
            for (int i = 0; i < headerCols; i++)
                fprintf(out, ",ORDER_%d", i);
            fprintf(out, "\n");
            headerWritten = true;
        }

        fprintf(out, "%u,%s,%s", node, zonePtr, type);
        for (int i = 0; i < headerCols; i++) {
            unsigned long v = (i < valueCount) ? values[i] : 0;
            fprintf(out, ",%lu", v);
        }
        fprintf(out, "\n");
        rowCount++;
    }

    if (!headerWritten)
        fprintf(out, "parse_status,no_rows_parsed\n");

    fclose(fp);
    return rowCount;
#undef FRAG_MAX_ORDERS
}

static int writeBuddyinfoCSV(FILE *out)
{
#define FRAG_MAX_ORDERS 32
    FILE *fp = NULL;
#ifdef TESTME
    if (isTestMode) {
        if (!testBuddyinfo[0])
            return -1;
        fp = fopen(testBuddyinfo, "r");
    } else {
        fp = fopen(BUDDYINFO_FILE, "r");
    }
#else
    fp = fopen(BUDDYINFO_FILE, "r");
#endif
    if (!fp)
        return -1;

    char line[1024];
    int rowCount = 0;
    int headerCols = 0;
    bool headerWritten = false;

    fprintf(out, "\n\nFragmentation_BuddyInfo:\n");

    while (fgets(line, sizeof(line), fp)) {
        unsigned node = 0;
        char zone[64] = {0};
        int consumed = 0;

        int matched = sscanf(line, "Node %u, zone %63s %n", &node, zone, &consumed);
        if (matched != 2 || consumed <= 0)
            continue;

        unsigned long values[FRAG_MAX_ORDERS] = {0};
        int valueCount = parseUnsignedSeries(line + consumed, values, FRAG_MAX_ORDERS);
        if (valueCount <= 0)
            continue;

        if (!headerWritten) {
            headerCols = valueCount;
            fprintf(out, "NODE,ZONE");
            for (int i = 0; i < headerCols; i++)
                fprintf(out, ",ORDER_%d", i);
            fprintf(out, "\n");
            headerWritten = true;
        }

        fprintf(out, "%u,%s", node, zone);
        for (int i = 0; i < headerCols; i++) {
            unsigned long v = (i < valueCount) ? values[i] : 0;
            fprintf(out, ",%lu", v);
        }
        fprintf(out, "\n");
        rowCount++;
    }

    if (!headerWritten)
        fprintf(out, "parse_status,no_rows_parsed\n");

    fclose(fp);
    return rowCount;
#undef FRAG_MAX_ORDERS
}

void saveFragmentationInfo(FILE *out)
{
    if (!out)
        return;

    if (writePagetypeInfoCSV(out) >= 0)
        return;

    (void)writeBuddyinfoCSV(out);
}

#ifdef ENABLE_CJSON
static int addPagetypeInfoJSON(cJSON_t *fragRoot)
{
#define FRAG_MAX_ORDERS 32
    FILE *fp = NULL;
#ifdef TESTME
    if (isTestMode) {
        if (!testPagetypeinfo[0])
            return -1;
        fp = fopen(testPagetypeinfo, "r");
    } else {
        fp = fopen(PGT_FILE, "r");
    }
#else
    fp = fopen(PGT_FILE, "r");
#endif
    if (!fp)
        return -1;

    cJSON_t *rows = g_cjson.CreateArray();
    if (!rows) {
        fclose(fp);
        return -1;
    }

    char line[1024];
    int pageBlockOrder = -1;
    int pagesPerBlock = -1;
    int rowCount = 0;

    while (fgets(line, sizeof(line), fp)) {
        if (pageBlockOrder < 0 && sscanf(line, "Page block order: %d", &pageBlockOrder) == 1)
            continue;
        if (pagesPerBlock < 0 && sscanf(line, "Pages per block: %d", &pagesPerBlock) == 1)
            continue;

        unsigned node = 0;
        char zone[64] = {0};
        char type[64] = {0};
        int consumed = 0;
        int matched = sscanf(line, "Node %u, zone %63[^,], type %63s %n", &node, zone, type, &consumed);
        if (matched != 3 || consumed <= 0)
            continue;

        char *zonePtr = zone;
        trimLeadingWhitespace(&zonePtr);
        trimTrailingWhitespace(zonePtr);

        unsigned long values[FRAG_MAX_ORDERS] = {0};
        int valueCount = parseUnsignedSeries(line + consumed, values, FRAG_MAX_ORDERS);
        if (valueCount <= 0)
            continue;

        cJSON_t *row = g_cjson.CreateObject();
        if (!row)
            continue;

        g_cjson.AddNumberToObject(row, "node", (double)node);
        g_cjson.AddStringToObject(row, "zone", zonePtr);
        g_cjson.AddStringToObject(row, "type", type);
        for (int i = 0; i < valueCount; i++) {
            char key[24];
            snprintf(key, sizeof(key), "order_%d", i);
            g_cjson.AddNumberToObject(row, key, (double)values[i]);
        }
        g_cjson.AddItemToArray(rows, row);
        rowCount++;
    }

    fclose(fp);

    if (pageBlockOrder >= 0)
        g_cjson.AddNumberToObject(fragRoot, "page_block_order", (double)pageBlockOrder);
    if (pagesPerBlock >= 0)
        g_cjson.AddNumberToObject(fragRoot, "pages_per_block", (double)pagesPerBlock);
    g_cjson.AddNumberToObject(fragRoot, "row_count", (double)rowCount);
    g_cjson.AddItemToObject(fragRoot, "rows", rows);
    return 0;
#undef FRAG_MAX_ORDERS
}

static int addBuddyinfoJSON(cJSON_t *fragRoot)
{
#define FRAG_MAX_ORDERS 32
    FILE *fp = NULL;
#ifdef TESTME
    if (isTestMode) {
        if (!testBuddyinfo[0])
            return -1;
        fp = fopen(testBuddyinfo, "r");
    } else {
        fp = fopen(BUDDYINFO_FILE, "r");
    }
#else
    fp = fopen(BUDDYINFO_FILE, "r");
#endif
    if (!fp)
        return -1;

    cJSON_t *rows = g_cjson.CreateArray();
    if (!rows) {
        fclose(fp);
        return -1;
    }

    char line[1024];
    int rowCount = 0;

    while (fgets(line, sizeof(line), fp)) {
        unsigned node = 0;
        char zone[64] = {0};
        int consumed = 0;
        int matched = sscanf(line, "Node %u, zone %63s %n", &node, zone, &consumed);
        if (matched != 2 || consumed <= 0)
            continue;

        unsigned long values[FRAG_MAX_ORDERS] = {0};
        int valueCount = parseUnsignedSeries(line + consumed, values, FRAG_MAX_ORDERS);
        if (valueCount <= 0)
            continue;

        cJSON_t *row = g_cjson.CreateObject();
        if (!row)
            continue;

        g_cjson.AddNumberToObject(row, "node", (double)node);
        g_cjson.AddStringToObject(row, "zone", zone);
        for (int i = 0; i < valueCount; i++) {
            char key[24];
            snprintf(key, sizeof(key), "order_%d", i);
            g_cjson.AddNumberToObject(row, key, (double)values[i]);
        }
        g_cjson.AddItemToArray(rows, row);
        rowCount++;
    }

    fclose(fp);
    g_cjson.AddNumberToObject(fragRoot, "row_count", (double)rowCount);
    g_cjson.AddItemToObject(fragRoot, "rows", rows);
    return 0;
#undef FRAG_MAX_ORDERS
}

void saveFragmentationInfo_JSON(cJSON_t *root)
{
    if (!root)
        return;

    cJSON_t *frag = g_cjson.CreateObject();
    if (!frag)
        return;

    if (addPagetypeInfoJSON(frag) == 0) {
        g_cjson.AddStringToObject(frag, "source", "pagetypeinfo");
        g_cjson.AddStringToObject(frag, "path", PGT_FILE);
    } else if (addBuddyinfoJSON(frag) == 0) {
        g_cjson.AddStringToObject(frag, "source", "buddyinfo");
        g_cjson.AddStringToObject(frag, "path", BUDDYINFO_FILE);
    } else {
        g_cjson.AddStringToObject(frag, "source", "none");
    }

    g_cjson.AddItemToObject(root, "fragmentation", frag);
}
#endif

void collectBandwidthData(FILE *out)
{
    if (!out)
        return;

    FILE *fp = NULL;
    char buffer[128] = {0};
    unsigned long totalBandwidth = 0;
    float usagePercentage = 0.0f;

    fp = fopen(BW_DDR_MODE_FILE, "r");
    if (!fp)
    {
        PRINT_ERROR("Failed to open %s: %s\n", BW_DDR_MODE_FILE, strerror(errno));
        return;
    }

    if (!fgets(buffer, sizeof(buffer), fp) || buffer[0] != '1')
    {
        fclose(fp);
        fp = fopen(BW_DDR_MODE_FILE, "w");
        if (!fp)
        {
            PRINT_ERROR("Failed to open %s for writing: %s\n", BW_DDR_MODE_FILE, strerror(errno));
            return;
        }
        if (fputs("1\n", fp) == EOF)
        {
            PRINT_ERROR("Failed to enable DDR bandwidth monitoring: %s\n", strerror(errno));
            fclose(fp);
            return;
        }
        fclose(fp);
        return;
    }
    fclose(fp);

    fp = fopen(BW_DDR_FILE, "r");
    if (!fp)
    {
        PRINT_ERROR("Failed to open %s: %s\n", BW_DDR_FILE, strerror(errno));
        return;
    }

    if (fgets(buffer, sizeof(buffer), fp) &&
        sscanf(buffer, "Total bandwidth: %lu KB/s, usage: %f%%", &totalBandwidth, &usagePercentage) == 2)
    {
        fprintf(out, "\n\nBandwidth:\nTotalBandwidth,UsagePercentage\n%lu,%.2f\n", totalBandwidth, usagePercentage);
    }
    else
    {
        PRINT_ERROR("Failed to parse bandwidth data from %s\n", BW_DDR_FILE);
    }

    fclose(fp);
}

/**
 * Reads the firmware image name from /version.txt.
 * Returns 1 if found, 0 otherwise. Writes result to fwName buffer.
 */

 int getFirmwareImageName(char *fwName, size_t fwNameLen)
 {
    if (!fwName || fwNameLen == 0)
    {
        PRINT_ERROR("Invalid parameter for firmware name\n");
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
        PRINT_ERROR("Invalid parameter\n");
        return 0;
    }
    struct ifreq ifr;
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1)
    {
        PRINT_ERROR("socket create failed: %s\n", strerror(errno));
        return 0;
    }
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1);
    ifr.ifr_addr.sa_family = AF_INET;
    if (ioctl(fd, SIOCGIFHWADDR, &ifr) == -1)
    {
        PRINT_ERROR("ioctl SIOCGIFHWADDR failed: %s (errno=%d)\n", strerror(errno), errno);
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
    DIR *proc = opendir(PROC_DIR);
    if (!proc)
        return 0;
    
    if (!procName || !pidOut) {
        closedir(proc);
        return 0;
    }
    
    struct dirent *entry;
    char commPath[PATH_MAX];
    char nameBuf[PATH_MAX];
    int found = 0;
    size_t procNameLen = strlen(procName);
    int maxEntries = 10000;  // Prevent unbounded iteration
    int entryCount = 0;
    
    while ((entry = readdir(proc)) != NULL && entryCount < maxEntries)
    {
        entryCount++;
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
            if (strncmp(nameBuf, procName, procNameLen + 1) == 0)
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
        PRINT_ERROR("Failed to open stat file for PID %u: %s\n", pid, strerror(errno));
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
    info->minFaults = minflt;
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
    if (!configPath || !config)
    {
        PRINT_ERROR("Invalid config input\n");
        return -1;
    }

    const char *dot = strrchr(configPath, '.');
    if (!dot || (strncmp(dot, ".conf", 6) != 0))
    {
        PRINT_ERROR("Invalid config file format: %s\n", configPath);
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
        PRINT_ERROR("Failed to open config file: %s\n", configPath);
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
            if (!config->whitelist)
            {
                PRINT_ERROR("Failed to allocate whitelist for %d entries\n", count);
                fclose(fp);
                return -1;
            }
            int idx = 0;
            int j = 0;
            char *tok = strtok(val, ",");
            while (tok && idx < count)
            {
                while (*tok == ' ')
                    tok++;
                config->whitelist[idx++] = strdup(tok);
                if (!config->whitelist[idx - 1])
                {
                    PRINT_ERROR("Failed to allocate whitelist entry\n");
                    for (j = 0; j < idx - 1; j++)
                        free(config->whitelist[j]);
                    free(config->whitelist);
                    config->whitelist = NULL;
                    config->whiteListCount = 0;
                    fclose(fp);
                    return -1;
                }
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
    while (tmp)
    {
        fprintf(output, "%u,%s,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu\n", tmp->pid, tmp->name, tmp->rssTotal, tmp->pssTotal,
                tmp->shared_clean_total, tmp->private_clean_total, tmp->private_dirty_total, tmp->swap_pss_total, tmp->minFaults, tmp->majFaults, tmp->cputime);
        tofree = tmp;
        tmp = tmp->next;
        free(tofree);
    }
    headProcessInfo = NULL;
    if (i != noOfPids)
    {
        PRINT_DBG("* Some process details might have been missed [%d vs actual %u]\n", i, noOfPids);
    }
}

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

#ifdef TESTME
/**
 * Frees the linked list and prints each node (for test mode).
 */
void checkAndFree()
{
    Process_Info *tmp = headProcessInfo, *tofree;
    int i = 1;
    while (tmp)
    {
        PRINT_MUST("%d,%u,%lu\n", i++, tmp->pid, tmp->pssTotal);
        tofree = tmp;
        tmp = tmp->next;
        free(tofree);
    }
    headProcessInfo = NULL;
}

/**
 * Test function to validate linked list insertion order.
 */
void testList()
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
unsigned prev_test_pss;
void testGetProcessInfos_Parse(char *tmp)
{
    unsigned test_rss = 0, test_pss = 0;
    unsigned test_shared_clean = 0, test_private_clean = 0, test_private_dirty = 0;
    unsigned test_swap_pss = 0;
    if (!strncmp(tmp, "Rss:", 4))
    {
        PRINT_DBG("%s", tmp);
        if (sscanf(tmp, "Rss: %u kB", &test_rss))
        {
            processInfoTest.rssTotal += test_rss;
        }
    }
    else if (!strncmp(tmp, "Pss:", 4))
    {
        PRINT_DBG("%s", tmp);
        if (sscanf(tmp, "Pss: %u kB", &test_pss))
        {
            processInfoTest.pssTotal += test_pss;
        }
        prev_test_pss = test_pss;
    }
    else if (!strncmp(tmp, "Shared_Clean:", 13))
    {
        PRINT_DBG("%s", tmp);
        if (sscanf(tmp, "Shared_Clean: %u kB", &test_shared_clean))
        {
         if (test_shared_clean) {
                processInfoTest.shared_clean_total += ((!smaps_rollup)? prev_test_pss : test_shared_clean);
         }
        }
    }
    else if (!strncmp(tmp, "Private_Clean:", 14))
    {
        PRINT_DBG("%s", tmp);
        if (sscanf(tmp, "Private_Clean: %u kB", &test_private_clean))
        {
            processInfoTest.private_clean_total += test_private_clean;
        }
    }
    else if (!strncmp(tmp, "Private_Dirty:", 14))
    {
        PRINT_DBG("%s", tmp);
        if (sscanf(tmp, "Private_Dirty: %u kB", &test_private_dirty))
        {
            processInfoTest.private_dirty_total += test_private_dirty;
        }
    }
    else if (!strncmp(tmp, "SwapPss:", 8))
    {
     PRINT_DBG("Test pss: %d %s\n", prev_test_pss, tmp);
        if (sscanf(tmp, "SwapPss: %u kB", &test_swap_pss))
        {
            processInfoTest.swap_pss_total += test_swap_pss;
        }
     test_rss = test_pss = 0;
    }
}

int testGetProcessInfos(FILE *smap, char *tmp)
{
    memset(&processInfoTest, 0, sizeof(processInfoTest));
    while (fgets(tmp, 255, smap))
    {
        testGetProcessInfos_Parse(tmp);
    }
    return 0;
}
#endif

// -----------------------------
// Process Info Parsing
// -----------------------------

// If use smaps_rollup, then job simple.
unsigned linesToSkipForRss_smaps_rollup = 0;
unsigned linesToSkipForPss_smaps_rollup = 0;
unsigned linesToSkipForSharedClean_smaps_rollup = 0;
unsigned linesToSkipForPrivateClean_smaps_rollup = 0;
unsigned linesToSkipForDirtyPrivate_smaps_rollup = 0;
unsigned linesToSkipForSwapPss_smaps_rollup = 0;

unsigned linesToSkipForRss = 0;
unsigned linesToSkipForPss = 0;
unsigned linesToSkipForSharedClean = 0;
unsigned linesToSkipForPrivateClean = 0;
unsigned linesToSkipForDirtyPrivate = 0;
unsigned linesToSkipForSwapPss = 0;
unsigned linesToSkipForRollover = 0;
unsigned linesToSkipIfRss0 = 0;
unsigned linesToSkipForSwapPssIfRss0 = 0;


/**
 * Parses /proc/[pid]/smaps_rollup for a given process and accumulates memory
 * statistics. Returns 0 on success, 1 on failure.
 */
int getProcessInfos_rollup_learnt(FILE *smap)
{
    char tmp[256];

    unsigned lines_To_skip = linesToSkipForRss_smaps_rollup;
    unsigned skipped = 1; // Tracks current skips
    unsigned expect_rss = 1;
    unsigned expect_pss = 0;
    unsigned expect_shared_clean = 0;
    unsigned expect_private_clean = 0;
    unsigned expect_private_dirty = 0;
    unsigned expect_swap_pss = 0;

    while (fgets(tmp, 256, smap))
    {
        /*
         * cat /proc/<pid>/smaps_rollup
         * 55aba70c6000-7ffe839f7000 ---p 00000000 00:00 0                          [rollup]
         * 
         * Rss:                4188 kB
         * Pss:                 853 kB
         * Pss_Anon:            816 kB
         * Pss_File:             37 kB
         * Pss_Shmem:             0 kB
         * Shared_Clean:       3372 kB
         * Shared_Dirty:          0 kB
         * Private_Clean:         0 kB
         * Private_Dirty:       816 kB
         * Referenced:         4188 kB
         * Anonymous:           816 kB
         * LazyFree:              0 kB
         * AnonHugePages:         0 kB
         * ShmemPmdMapped:        0 kB
         * FilePmdMapped:        0 kB
         * Shared_Hugetlb:        0 kB
         * Private_Hugetlb:       0 kB
         * Swap:                  0 kB
         * SwapPss:               0 kB
         * Locked:                0 kB
            */

#ifdef TESTME
        testGetProcessInfos_Parse(tmp);
#endif
        unsigned rss = 0, pss = 0;
        unsigned shared_clean = 0, private_clean = 0, private_dirty = 0;
        unsigned swap_pss = 0;
        if (++skipped > lines_To_skip)
        {
            if (expect_rss)
            {
                if (sscanf(tmp, "Rss: %u kB", &rss))
                {
                    PRINT_DBG_SCANNED("Read Rss (%u) after %u/%u lines --> %s\r", rss, skipped,
                                          lines_To_skip, tmp);
                    getProcessInfo.rssTotal = rss;
                    PRINT_DBG_SCANNED("rssTotal (%lu)\n", getProcessInfo.rssTotal);
                    lines_To_skip = linesToSkipForPss_smaps_rollup;
                    expect_pss = 1;
                    expect_rss = 0;
                    skipped = 1;
                    continue;
                }
            }
            else if (expect_pss)
            {
                if (sscanf(tmp, "Pss: %u kB", &pss))
                {
                    PRINT_DBG_SCANNED("Read Pss (%u) after %u/%u lines  --> %s\r", pss, skipped, lines_To_skip,
                                      tmp);
                    getProcessInfo.pssTotal = pss;
                    lines_To_skip = linesToSkipForSharedClean_smaps_rollup;
                    expect_shared_clean = 1;
                    skipped = 1;
                    expect_pss = 0;
                    continue;
                }
            }
            else if (expect_shared_clean)
            {
                if (sscanf(tmp, "Shared_Clean: %u kB", &shared_clean))
                {
                    PRINT_DBG_SCANNED("Read shared_clean (%u)  %u/%u lines --> %s\r", shared_clean, skipped,
                                      lines_To_skip, tmp);
            // %% This is the main reason as to why we are not reading form smaps_rollup
                    getProcessInfo.shared_clean_total = shared_clean;
                    expect_private_clean = 1;
                    lines_To_skip = linesToSkipForPrivateClean_smaps_rollup;
                    expect_shared_clean = 0;
                    skipped = 1;
                    continue;
                }
            }
            else if (expect_private_clean)
            {
                if (sscanf(tmp, "Private_Clean: %u kB", &private_clean))
                {
                    expect_private_clean = 0;
                    PRINT_DBG_SCANNED("Read private_clean (%u)  %u/%u lines --> %s\r", private_clean, skipped,
                                      lines_To_skip, tmp);
                    getProcessInfo.private_clean_total = private_clean;
                    skipped = 1;
                    expect_private_dirty = 1;
                    lines_To_skip = linesToSkipForDirtyPrivate_smaps_rollup;
                    continue;
                }
            }
            else if (expect_private_dirty)
            {
                if (sscanf(tmp, "Private_Dirty: %u kB", &private_dirty))
                {
                    expect_private_dirty = 0;
                    PRINT_DBG_SCANNED("Read private_dirty (%u)  %u/%u lines --> %s\r", private_dirty, skipped,
                                      lines_To_skip, tmp);
                    getProcessInfo.private_dirty_total = private_dirty;
                    skipped = 1;
                    if (0xFFFF != linesToSkipForSwapPss_smaps_rollup)
                    {
                        expect_swap_pss = 1;
                        lines_To_skip = linesToSkipForSwapPss_smaps_rollup;
            continue;
                    }
                    break;
                }
            }
            else if (expect_swap_pss)
            {
                if (sscanf(tmp, "SwapPss: %u kB", &swap_pss))
                {
                    PRINT_DBG_SCANNED("Read swap_pss (%u)  %u/%u lines --> %s\r", swap_pss, skipped,
                                      lines_To_skip, tmp);
                    getProcessInfo.swap_pss_total = swap_pss;
                    break;
                }
            }
        }
        else
        {
            continue;
        }
    }
    return 0;
}

int getProcessInfos_rollup(FILE *smap)
{
    char tmp[256];
    unsigned expect_rss = 1;
    unsigned expect_pss = 0;
    unsigned expect_shared_clean = 0;
    unsigned expect_private_clean = 0;
    unsigned expect_private_dirty = 0;
    unsigned expect_swap_pss = 0;

    unsigned linesSkippedForRss = 0;
    unsigned linesSkippedForPss = 0;
    unsigned linesSkippedForSharedClean = 0;
    unsigned linesSkippedForPrivateClean = 0;
    unsigned linesSkippedForDirtyPrivate = 0;
    unsigned linesSkippedForSwapPss = 0;

    while (fgets(tmp, 256, smap))
    {
#ifdef TESTME
    testGetProcessInfos_Parse(tmp);
#endif
        unsigned rss = 0, pss = 0;
        unsigned shared_clean = 0, private_clean = 0, private_dirty = 0;
        unsigned swap_pss = 0;
        if (!linesToSkipForRss_smaps_rollup)
        {
            if (sscanf(tmp, "Rss: %u kB", &rss))
            {
                getProcessInfo.rssTotal = rss;
                linesToSkipForRss_smaps_rollup = linesSkippedForRss + 1;
                PRINT_DBG_INITIAL("After %u lines Read Rss (%u) in --> %s\r", linesToSkipForRss_smaps_rollup, rss, tmp);
            }
            else
            {
                linesSkippedForRss++;
            }
        }
        else if (!linesToSkipForPss_smaps_rollup)
        {
            if (sscanf(tmp, "Pss: %u kB", &pss))
            {
                getProcessInfo.pssTotal = pss;
                linesToSkipForPss_smaps_rollup = linesSkippedForPss + 1;
                PRINT_DBG_INITIAL("After %u lines Read Pss (%u) in --> %s\r", linesToSkipForPss_smaps_rollup, pss, tmp);
            }
            else
            {
                linesSkippedForPss++;
            }
        }
        else if (!linesToSkipForSharedClean_smaps_rollup)
        {
            if (sscanf(tmp, "Shared_Clean: %u kB", &shared_clean))
            {
                getProcessInfo.shared_clean_total = shared_clean;
                linesToSkipForSharedClean_smaps_rollup = linesSkippedForSharedClean + 1;
                PRINT_DBG_INITIAL("After %u lines Read Shared_Clean (%u) in --> %s\r",
                                  linesToSkipForSharedClean_smaps_rollup, shared_clean, tmp);
            }
            else
            {
                linesSkippedForSharedClean++;
            }
        }
        else if (!linesToSkipForPrivateClean_smaps_rollup)
        {
            if (sscanf(tmp, "Private_Clean: %u kB", &private_clean))
            {
                getProcessInfo.private_clean_total = private_clean;
                linesToSkipForPrivateClean_smaps_rollup = linesSkippedForPrivateClean + 1;
                PRINT_DBG_INITIAL("After %u lines Read PrivateClean (%u) in --> %s\r",
                                  linesToSkipForPrivateClean_smaps_rollup, private_clean, tmp);
            }
            else
            {
                linesSkippedForPrivateClean++;
            }
        }
        else if (!linesToSkipForDirtyPrivate_smaps_rollup)
        {
            if (sscanf(tmp, "Private_Dirty: %u kB", &private_dirty))
            {
                getProcessInfo.private_dirty_total = private_dirty;
                linesToSkipForDirtyPrivate_smaps_rollup = linesSkippedForDirtyPrivate + 1;
                PRINT_DBG_INITIAL("After %u lines Read DirtyPrivate (%u) in --> %s\r",
                                  linesToSkipForDirtyPrivate_smaps_rollup, private_dirty, tmp);
            }
            else
            {
                linesSkippedForDirtyPrivate++;
            }
        }
        else if (!linesToSkipForSwapPss_smaps_rollup)
        {
            if (sscanf(tmp, "SwapPss: %u kB", &swap_pss))
            {
                getProcessInfo.swap_pss_total = swap_pss;
                linesToSkipForSwapPss_smaps_rollup = linesSkippedForSwapPss + 1;
                PRINT_DBG_INITIAL("After %u lines Read Swap_Pss (%u) in --> %s\r", linesToSkipForSwapPss_smaps_rollup,
                                  swap_pss, tmp);
            }
            else
            {
                // check if smap doesn't contain swappss..
                if (sscanf(tmp, "Locked: %u kB", &swap_pss))
                {
                    PRINT_DBG_INITIAL("*****No SwapPss....skipping\n");
                linesToSkipForSwapPss_smaps_rollup = 0xFFFF;
                }
                else
                {
                    linesSkippedForSwapPss++;
                continue;
                }
            }
            getProcessInfos_ptr = getProcessInfos_rollup_learnt;
            PRINT_DBG_INITIAL("Using getProcessInfos_rollup_learnt, without SwapPss\n");
            break;
        }
        else
        {
            PRINT_ERROR("%s-%d: Shouldn't be here expect rss:pss:shared_clean:priv_clean:priv_dirty:swap \
                        %u:%u:%u:%u:%u:%u read line %s", __FUNCTION__, __LINE__, expect_rss, expect_pss, expect_shared_clean, \
                        expect_private_clean, expect_private_dirty, expect_swap_pss, tmp);
            return 1;
        }
    }
    return 0;
}

/**
 * Parses /proc/[pid]/smaps for a given process and accumulates memory
 * statistics. Returns 0 on success, 1 on failure.
 */
int getProcessInfos_learnt(FILE *smap)
{
    char tmp[256];
    
    unsigned lines_To_skip = linesToSkipForRss;
    unsigned skipped = 1; // Tracks current skips
    unsigned expect_rss = 1;
    unsigned expect_pss = 0;
    unsigned expect_shared_clean = 0;
    unsigned expect_private_clean = 0;
    unsigned expect_private_dirty = 0;
    unsigned expect_swap_pss = 0;
    unsigned prev_pss = 0;

    while (fgets(tmp, 255, smap))
    {
#ifdef TESTME
        testGetProcessInfos_Parse(tmp);
#endif
        unsigned rss = 0, pss = 0;
        unsigned shared_clean = 0, private_clean = 0, private_dirty = 0;
        unsigned swap_pss = 0;
	// This block repeats in getProcessInfos_initial...is redundant, but efficient
        if (++skipped > lines_To_skip)
        {
            if (expect_rss)
            {
                if (sscanf(tmp, "Rss: %u kB\n", &rss))
                {
                    if (rss)
                    {
                        getProcessInfo.rssTotal += rss;
                        lines_To_skip = linesToSkipForPss;
                        expect_pss = 1;
                        expect_rss = 0;
                    }
                    else if (linesToSkipForSwapPss)
                    {
                        lines_To_skip = linesToSkipForSwapPssIfRss0;
                        expect_rss = 0;
                        expect_swap_pss = 1;
                    }
                    else
                    {
                        lines_To_skip = linesToSkipIfRss0;
                    }
                    skipped = 1;
                    continue;
                }
            }
            else if (expect_pss)
            {
                if (sscanf(tmp, "Pss: %u kB\n", &pss))
                {
                    getProcessInfo.pssTotal += pss;
                    lines_To_skip = linesToSkipForSharedClean;
                    expect_shared_clean = 1;
                    skipped = 1;
                    expect_pss = 0;
                    prev_pss = pss;
                    continue;
                }
            }
            else if (expect_shared_clean)
            {
                if (sscanf(tmp, "Shared_Clean: %u kB\n", &shared_clean))
                {
                    if (shared_clean) {
                        getProcessInfo.shared_clean_total += prev_pss;
                    }

                    expect_private_clean = 1;
                    lines_To_skip = linesToSkipForPrivateClean;
                    expect_shared_clean = 0;
                    skipped = 1;
                    continue;
                }
            }
            else if (expect_private_clean)
            {
                if (sscanf(tmp, "Private_Clean: %u kB\n", &private_clean))
                {
                    expect_private_clean = 0;
                    getProcessInfo.private_clean_total += private_clean;
                    skipped = 1;
                    expect_private_dirty = 1;
                    lines_To_skip = linesToSkipForDirtyPrivate;
                    continue;
                }
            }
            else if (expect_private_dirty)
            {
                if (sscanf(tmp, "Private_Dirty: %u kB\n", &private_dirty))
                {
                    expect_private_dirty = 0;
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
                if (sscanf(tmp, "SwapPss: %u kB\n", &swap_pss))
                {
                    expect_swap_pss = 0;
                    expect_rss = 1;
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
    return 0;
}
int getProcessInfos_initial(FILE *smap)
{
    char tmp[256];

    unsigned lines_To_skip = linesToSkipForRss;
    unsigned skipped = 1; // Tracks current skips
    unsigned expect_rss = 1;
    unsigned expect_pss = 0;
    unsigned expect_shared_clean = 0;
    unsigned expect_private_clean = 0;
    unsigned expect_private_dirty = 0;
    unsigned expect_swap_pss = 0;
    unsigned prev_pss = 0;

    unsigned linesSkippedForRss = 0;
    unsigned linesSkippedForPss = 0;
    unsigned linesSkippedForSharedClean = 0;
    unsigned linesSkippedForPrivateClean = 0;
    unsigned linesSkippedForDirtyPrivate = 0;
    unsigned linesSkippedForSwapPss = 0;
    unsigned linesSkippedForRollover = 0;

    while (fgets(tmp, 256, smap))
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

#ifdef TESTME
        testGetProcessInfos_Parse(tmp);
#endif
        unsigned rss = 0, pss = 0;
        unsigned shared_clean = 0, private_clean = 0, private_dirty = 0;
        unsigned swap_pss = 0;
        if (!linesToSkipForRollover)
        { // Learn here
            if (!linesToSkipForRss)
            {
                if (sscanf(tmp, "Rss: %u kB", &rss))
                {
                    getProcessInfo.rssTotal += rss;
                    linesToSkipForRss = linesSkippedForRss + 1;
                    PRINT_DBG_INITIAL("After %u lines Read Rss (%u) in --> %s\r", linesToSkipForRss, rss, tmp);
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
                    PRINT_DBG_INITIAL("After %u lines Read Pss (%u) in --> %s\r", linesToSkipForPss, pss, tmp);
                    prev_pss = pss;
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
                    if (shared_clean) {
                        getProcessInfo.shared_clean_total += prev_pss;
                    }
                    linesToSkipForSharedClean = linesSkippedForSharedClean + 1;
                    PRINT_DBG_INITIAL("After %u lines Read Shared_Clean (%u) in --> %s\r",
                                      linesToSkipForSharedClean, shared_clean, tmp);
                }
                else
                {
                    linesSkippedForSharedClean++;
                }
            }
            else if (!linesToSkipForPrivateClean)
            {
                if (sscanf(tmp, "Private_Clean: %u kB", &private_clean))
                {
                    getProcessInfo.private_clean_total += private_clean;
                    linesToSkipForPrivateClean = linesSkippedForPrivateClean + 1;
                    PRINT_DBG_INITIAL("After %u lines Read PrivateClean (%u) in --> %s\r",
                                      linesToSkipForPrivateClean, private_clean, tmp);
                }
                else
                {
                    linesSkippedForPrivateClean++;
                }
            }
            else if (!linesToSkipForDirtyPrivate)
            {
                if (sscanf(tmp, "Private_Dirty: %u kB", &private_dirty))
                {
                    getProcessInfo.private_dirty_total += private_dirty;
                    linesToSkipForDirtyPrivate = linesSkippedForDirtyPrivate + 1;
                    PRINT_DBG_INITIAL("After %u lines Read DirtyPrivate (%u) in --> %s\r",
                                      linesToSkipForDirtyPrivate, private_dirty, tmp);
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
                    PRINT_DBG_INITIAL("After %u lines Read Swap_Pss (%u) in --> %s\r", linesToSkipForSwapPss,
                                      swap_pss, tmp);
                    linesToSkipForSwapPssIfRss0 = linesToSkipForPss + linesToSkipForSharedClean + linesToSkipForPrivateClean + linesToSkipForDirtyPrivate + linesToSkipForSwapPss;
                }
                else
                {
                    // check if smap doesn't contain swappss..
                    if (sscanf(tmp, "Rss: %u kB", &rss))
                    {
                        getProcessInfo.rssTotal += rss;
                        linesToSkipForRollover = linesSkippedForSwapPss + 1;
                        PRINT_DBG_INITIAL("*****No SwapPss....skipping\n");
                        PRINT_DBG_INITIAL("No SwapPss...After %u lines Read Rss (%u) in --> %s\r", linesToSkipForRollover, rss,
                                          tmp);
                        lines_To_skip = linesToSkipForPss;
                        expect_pss = 1;
                        expect_rss = 0;
                        linesToSkipIfRss0 = linesToSkipForPss + linesToSkipForSharedClean + linesToSkipForPrivateClean +
                                            linesToSkipForDirtyPrivate + linesToSkipForRollover - 4;
                        getProcessInfos_ptr = getProcessInfos_learnt;
                        PRINT_DBG_INITIAL("Using getProcessInfos_learnt, without SwapPss\n");
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
                    PRINT_DBG_INITIAL("After %u lines Read Rss (%u) in --> %s\r", linesToSkipForRollover, rss, tmp);
                    lines_To_skip = linesToSkipForPss;
                    expect_pss = 1;
                    expect_rss = 0;
                    linesToSkipIfRss0 = linesToSkipForPss + linesToSkipForSharedClean + linesToSkipForDirtyPrivate + linesToSkipForPrivateClean +
                                        linesToSkipForSwapPss + linesToSkipForRollover - 5;
                    getProcessInfos_ptr = getProcessInfos_learnt;
                }
                else
                {
                    linesSkippedForRollover++;
                }
            }
            else
            {
            PRINT_ERROR("%s-%d: Shouldn't be here expect rss:pss:shared_clean:priv_clean:priv_dirty:swap \
                            %u:%u:%u:%u:%u:%u read line %s", __FUNCTION__, __LINE__, expect_rss, expect_pss, expect_shared_clean, \
                            expect_private_clean, expect_private_dirty, expect_swap_pss, tmp);
            return 1;
            }
        }
        else
        { // Learnt the format...instead of calling getProcessInfos_learnt, which involves some more bookkeeping..
            //return getProcessInfos_learnt(smap);
	    // This block repeats in getProcessInfos_learnt...is redundant, but efficient
            if (++skipped > lines_To_skip)
            {
                if (expect_rss)
                {
                    if (sscanf(tmp, "Rss: %u kB", &rss))
                    {
                        PRINT_DBG_SCANNED("Read Rss (%u) after %u/%u lines --> %s\r", rss, skipped,
                                              lines_To_skip, tmp);
                        if (rss)
                        {
                            getProcessInfo.rssTotal += rss;
                            PRINT_DBG_SCANNED("rssTotal (%lu)\n", getProcessInfo.rssTotal);
                            lines_To_skip = linesToSkipForPss;
                            expect_pss = 1;
                            expect_rss = 0;
                        }
                        else if (linesToSkipForSwapPss)    
                        {
                            lines_To_skip = linesToSkipForSwapPssIfRss0;
                            expect_rss = 0;
                            expect_swap_pss = 1;
                        }
                        else
                        {
                            lines_To_skip = linesToSkipIfRss0;
                        }
                        skipped = 1;
                        continue;
                    }
                }
                else if (expect_pss)
                {
                    if (sscanf(tmp, "Pss: %u kB", &pss))
                    {
                        PRINT_DBG_SCANNED("Read Pss (%u) after %u/%u lines  --> %s\r", pss, skipped, lines_To_skip,
                                          tmp);
                        getProcessInfo.pssTotal += pss;
                        lines_To_skip = linesToSkipForSharedClean;
                        expect_shared_clean = 1;
                        skipped = 1;
                        expect_pss = 0;
                        prev_pss = pss;
                        continue;
                    }
                }
                else if (expect_shared_clean)
                {
                    if (sscanf(tmp, "Shared_Clean: %u kB", &shared_clean))
                    {
                        PRINT_DBG_SCANNED("Read shared_clean (%u)  %u/%u lines --> %s\r", shared_clean, skipped,
                                          lines_To_skip, tmp);
                        // %% This is the main reason as to why we are not reading form smaps_rollup
                        if (shared_clean) {
                            getProcessInfo.shared_clean_total += prev_pss;
                        }
                        expect_private_clean = 1;
                        lines_To_skip = linesToSkipForPrivateClean;
                        expect_shared_clean = 0;
                        skipped = 1;
                        continue;
                    }
                }
                else if (expect_private_clean)
                {
                    if (sscanf(tmp, "Private_Clean: %u kB", &private_clean))
                    {
                        expect_private_clean = 0;
                        PRINT_DBG_SCANNED("Read private_clean (%u)  %u/%u lines --> %s\r", private_clean, skipped,
                                          lines_To_skip, tmp);
                        getProcessInfo.private_clean_total += private_clean;
                        skipped = 1;
                        expect_private_dirty = 1;
                        lines_To_skip = linesToSkipForDirtyPrivate;
                        continue;
                    }
                }
                else if (expect_private_dirty)
                {
                    if (sscanf(tmp, "Private_Dirty: %u kB", &private_dirty))
                    {
                        expect_private_dirty = 0;
                        PRINT_DBG_SCANNED("Read private_dirty (%u)  %u/%u lines --> %s\r", private_dirty, skipped,
                                          lines_To_skip, tmp);
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
                    if (sscanf(tmp, "SwapPss: %u kB", &swap_pss))
                    {
                        expect_swap_pss = 0;
                        expect_rss = 1;
                        PRINT_DBG_SCANNED("Read swap_pss (%u)  %u/%u lines --> %s\r", swap_pss, skipped,
                                          lines_To_skip, tmp);
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
        } // else of if (!linesToSkipForRollover)
    } // while (fgets(tmp, 256, smap))
    return 0;
}

int getProcessInfos(unsigned pid)
{
    int ret = 1;
    char tmp[128];
    FILE *smap = NULL;
#ifdef TESTME
    memset(&processInfoTest, 0, sizeof(processInfoTest));
    prev_test_pss = 0;
    if (isTestMode)
    {
        memcpy(tmp, testSmap, 128);
        if (strstr(testSmap, "smaps_rollup") != NULL)
        {
            smaps_rollup = 1;
            getProcessInfos_ptr = getProcessInfos_rollup;
        }
        else
        {
            smaps_rollup = 0;
            getProcessInfos_ptr = getProcessInfos_initial;
        }
        PRINT_MUST("%s: Testing with %s\n", __FUNCTION__, tmp);
        smap = fopen(tmp, "r");
    }
    else
#endif
    {
        if (force_smaps)
        {
            smaps_rollup = 0;
            strcpy(gSMAPS_OR_ROLLUP, "smaps");
            getProcessInfos_ptr = getProcessInfos_initial;
            snprintf(tmp, sizeof(tmp), "/proc/%u/smaps", pid);
            smap = fopen(tmp, "r");
        }
        else
        {
            smaps_rollup = 1;
            strcpy(gSMAPS_OR_ROLLUP, "smaps_rollup");
            getProcessInfos_ptr = getProcessInfos_rollup;
            snprintf(tmp, sizeof(tmp), "/proc/%u/smaps_rollup", pid);
            smap = fopen(tmp, "r");

            if (!smap)
            {
                smaps_rollup = 0;
                strcpy(gSMAPS_OR_ROLLUP, "smaps");
                getProcessInfos_ptr = getProcessInfos_initial;
                snprintf(tmp, sizeof(tmp), "/proc/%u/smaps", pid);
                smap = fopen(tmp, "r");
            }
        }
    }

    if (smap)
    {
        ret = getProcessInfos_ptr(smap);
        fclose(smap);
#ifdef TESTME
        if ((getProcessInfo.rssTotal != processInfoTest.rssTotal) || (getProcessInfo.pssTotal != processInfoTest.pssTotal) ||
            (getProcessInfo.shared_clean_total != processInfoTest.shared_clean_total) || 
            (getProcessInfo.private_clean_total != processInfoTest.private_clean_total) || 
            (getProcessInfo.private_dirty_total != processInfoTest.private_dirty_total) ||
            (getProcessInfo.swap_pss_total != processInfoTest.swap_pss_total))
        {
            printf("something went wrong while processing smap for pid %d\n", pid);
            printf("%lu:%lu, %lu:%lu, %lu:%lu, %lu:%lu, %lu:%lu, %lu:%lu\n", 
                   getProcessInfo.rssTotal, processInfoTest.rssTotal,getProcessInfo.pssTotal,processInfoTest.pssTotal,
                   getProcessInfo.shared_clean_total, processInfoTest.shared_clean_total, 
                   getProcessInfo.private_clean_total, processInfoTest.private_clean_total, 
                   getProcessInfo.private_dirty_total, processInfoTest.private_dirty_total, getProcessInfo.swap_pss_total, processInfoTest.swap_pss_total);
            unitTestFailed = 1;
        }
#endif
    }
    else {
        PRINT_ERROR("%s: Open failed, errno %d [%s]\n", tmp, errno, strerror(errno));
        ret = 1;
    }
    return ret;
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
void printHelpAndUsage(char *argv[], bool moreInfo, int returnCode)
{
    printf("%s (v%s)\n\n", MEMINSIGHT_BIN, memInsightVersion);
    printf("Usage: %s [OPTIONS]\n", MEMINSIGHT_BIN);
    printf("A lightweight, configurable tool for collecting detailed system and per-process memory and CPU statistics.\n\n");

    printf("Options:\n");
    printf("  -a, --all                             Include kernel threads for process monitoring\n");
    printf("  -c, --config <file>                   Path to configuration file with %s extension\n", CONFIG_EXTN);
    printf("  -o, --output <directory>              Output directory for generated report files (default: %s)\n", DEFAULT_OUT_DIR);
    printf("      --interval <seconds>              Interval in seconds between iterations (overrides config)\n");
    printf("      --iterations <count>              Number of iterations to run (overrides config)\n");
    printf("  -s, --smaps               Force /proc/<pid>/smaps (disable auto smaps_rollup detection)\n");
    printf("  -h, --help                            Show this help message and exit\n");
#ifdef TESTME
    printf("  -t, --test <smapsFile> <meminfoFile> [buddyinfoFile] [pagetypeinfoFile]\n");
    printf("                                      Run in test mode using supplied sample files\n\n");
#endif
#ifdef ENABLE_CJSON
    printf("      --fmt <format>                    Specify report format: csv (default) or json\n");
    printf("                                        (cJSON loaded at runtime via dlopen)\n");
    printf("      --json-pretty                     Pretty-print JSON output (only with --fmt json)\n\n");
#endif

    if (moreInfo)
    {
        printf("\nPrecedence:\n");
        printf("  Command-line arguments override config file values, which override built-in defaults.\n\n");

        printf("Default behavior (no flags):\n");
        printf("  - Runs indefinite number of iterations, with an interval of 15 minutes, monitors all processes with log level INFO\n");
        printf("  - Output: /tmp/<MAC>_<timestamp>_iter<iteration>_%s\n\n", CSV_FILE_NAME);

        printf("Example:\n");
        printf("  %s\n", argv[0]);
        printf("  %s --all\n", argv[0]);
        printf("  %s --config /etc/meminsight_configuration%s\n", argv[0], CONFIG_EXTN);
        printf("  %s -c myconfig%s -a --interval 10 --iterations 5\n", argv[0], CONFIG_EXTN);
        printf("  %s --output /var/log/ --iterations 3\n", argv[0]);
#ifdef TESTME
    printf("  %s --test ../tst/smaps.txt ../tst/meminfo.txt\n", argv[0]);
    printf("  %s --test ../tst/smaps.txt ../tst/meminfo.txt ../tst/buddyinfo.txt ../tst/pagetypeinfo.txt\n\n", argv[0]);
#endif

        printf("Sample config file:\n");
        printf("  process_whitelist=myapp,systemd,1234\n  output_dir=/var/log/\n  iterations=10\n  interval=60\n  log_level=INFO\n\n");

        printf("Notes:\n");
        printf("  - Supported config file extensions: %s\n", CONFIG_EXTN);
        printf("  - If both interval and iterations are set via CLI, both are used.\n");
        printf("  - If only one is set, the other uses its default or config value.\n");
        printf("  - Output file name format: <MAC>_<TIMESTAMP>_iter<iteration>_%s\n", CSV_FILE_NAME);
    }
    exit(returnCode);
}

/**
 * Collects system-wide memory statistics and writes to output file.
 * Returns 0 on success, -1 on failure.
 */
int collectSystemMemoryStats(bool enableKThreads, const char *outDir, int iterations, int interval, bool long_run)
{
    // Initialize setup info (MAC, firmware name, output dir, file extension)
    SetupInfo setup = initializeSetupInfo(outDir, g_reportFormat);
    if (!setup.dirCreated) {
        PRINT_MUST("Failed to create output directory: %s\n", setup.outputDir);
        return -1;
    }

    PRINT_MUST("Capturing System wide stats into directory %s\n", setup.outputDir);
    char outputfile[512];

    for (int iter = 0; long_run || iter < iterations; iter++)
    {
        PRINT_INFO("\n==== Iteration %d%s ====\n", iter + 1, long_run ? "/∞" : "");
        unsigned int noOfPids = 0;

        // Current timestamp
        time_t timenow = time(NULL);
        struct tm *tm_info = localtime(&timenow);
        char timestamp[32] = {0};
        char ts[32] = {0};
        strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", tm_info);
        strftime(timestamp, sizeof(timestamp), "%Y%m%d%H%M%S", tm_info);

        snprintf(outputfile, sizeof(outputfile), "%s/%s_%s_iter%d_%s",
                 setup.outputDir, setup.mac, timestamp, iter + 1, setup.reportFileName);
        PRINT_INFO("Capturing Process stats into: %s\n", outputfile);

        /* Recalculate uptime fresh on each iteration. */
        const char *uptime = getSystemUptime();

#ifdef ENABLE_CJSON
        if (g_reportFormat == REPORT_JSON) {
            g_rootObject = g_cjson.CreateObject();
            if (!g_rootObject) {
                PRINT_ERROR("Failed to create JSON root object\n");
                return -1;
            }
            g_cjson.AddStringToObject(g_rootObject, "firmware_name", setup.fwName);
            g_cjson.AddStringToObject(g_rootObject, "mac_address",   setup.mac);
            g_cjson.AddStringToObject(g_rootObject, "timestamp",     ts);
            g_cjson.AddStringToObject(g_rootObject, "uptime",        uptime);
            g_cjson.AddStringToObject(g_rootObject, "kernel_version", setup.kernelVersion);
            g_cjson.AddStringToObject(g_rootObject, "report_version", reportVersion);
        }
#endif

        FILE *output = NULL;
        if (g_reportFormat == REPORT_CSV) {
            output = fopen(outputfile, "w");
            if (NULL == output) {
                PRINT_MUST("%s: Open failed, %d [%s]\n", outputfile, errno, strerror(errno));
                return -1;
            }
            fprintf(output, "%s\n", CSV_META_HEADER);
            fprintf(output, "%s,%s,%s,%s,%s,%s\n\n", setup.fwName, setup.mac, ts, uptime, setup.kernelVersion, reportVersion);
        }

        unsigned long rssTotal = 0, pssTotal = 0, shared_clean_total = 0, private_clean_total = 0, private_dirty_total = 0, swap_pss_total = 0;
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
                            PRINT_ERROR("Failed to fill process stat fields for PID %d\n", pid);
                            continue;
                        }
                        if (flags & PF_KTHREAD && !enableKThreads)
                        {             /*PF_KTHREAD*/
                            continue; // Skip kernel threads if not included
                        }

                        if (!getProcessInfos(pid))
                        {
#ifdef TESTME
                            if (isTestMode) {
				    if (1 >= isTestMode) {
					    break;
				    }else {
					    isTestMode--;
				    }
                            }
#endif                
                            getProcessInfo.pid = pid;
                            // name, majFaults, cputime already filled by
                            // fillProcessStatFields
                            rssTotal += getProcessInfo.rssTotal;
                            pssTotal += getProcessInfo.pssTotal;
                            shared_clean_total += getProcessInfo.shared_clean_total;
                            private_clean_total += getProcessInfo.private_clean_total;
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
            PRINT_ERROR("Failed to open %s directory: %s\n", PROC_DIR, strerror(errno));
            if (output) fclose(output);
#ifdef ENABLE_CJSON
            if (g_reportFormat == REPORT_JSON && g_rootObject) {
                g_cjson.Delete(g_rootObject);
                g_rootObject = NULL;
            }
#endif
            return -1;
        }

        PRINT_INFO("\nProcessed %u processes\n>> RSS_Total %lu\n>> PSS_Total "
               "%lu\n>> Shared_Clean_Total %lu\n>> Private_Clean_Total %lu\n>> Private_Dirty_Total %lu\n",
               noOfPids, rssTotal, pssTotal, shared_clean_total, private_clean_total, private_dirty_total);

        if (g_reportFormat == REPORT_CSV) {
            saveMeminfo(output);
            saveFragmentationInfo(output);
    #ifdef TESTME
            if (isTestMode && unitTestFailed) {
                fclose(output);
                return -1;
            }
    #endif
            fprintf(output, "\n\nProcesses:\nPID,EXE,RSS,PSS,SHARED_CLEAN,PRIVATE_CLEAN,PRIVATE_"
                            "DIRTY,SWAP_PSS,MIN_FAULTS,MAJ_FAULTS,CPU_TIME\n");
            writeProcessInfo(noOfPids, output);
            fprintf(output, "0,Total,%lu,%lu,%lu,%lu,%lu,%lu,0,0,0\n", rssTotal, pssTotal, shared_clean_total,
                    private_clean_total, private_dirty_total, swap_pss_total);
            if (g_bwDataAvailable)
                collectBandwidthData(output);
            fclose(output);
        }
#ifdef ENABLE_CJSON
        else if (g_reportFormat == REPORT_JSON) {
            saveMeminfo_JSON(g_rootObject);
            saveFragmentationInfo_JSON(g_rootObject);
            cJSON_t *processesArray = g_cjson.CreateArray();
            if (processesArray) {
                writeProcessInfo_JSON(processesArray);
                g_cjson.AddItemToObject(g_rootObject, "processes", processesArray);
            }
            if (writeJSONToFile(outputfile, &setup) != 0) {
                PRINT_ERROR("Failed to write JSON output file: %s\n", outputfile);
                return -1;
            }
        }
#endif
#ifdef TESTME
        if (1 == isTestMode) {
            break;
        }
#endif
        if (interval >= 0 && (long_run || iter + 1 < iterations))
        {
            PRINT_INFO("* %d%s Iteration completed. Sleeping for %d seconds before next iteration... \n", iter + 1, long_run ? "/∞" : "", interval);
            sleep(interval);
        }
    }
    printf("\n---- Completed Data Capture ----\n");
    return 0;
}

int handleConfigMode(const char *confFile, const char *cli_out_dir, int cli_iterations, int cli_interval, bool enableKThreads, bool long_run)
{
    Config_Data config = {0};
    if (parseConfig(confFile, &config) != 0)
    {
        PRINT_ERROR("Error: Failed to parse config file '%s'\n", confFile);
        return -1;
    }

    // Output directory: CLI > config > default
    const char *final_out_dir = (cli_out_dir[0] && strncmp(cli_out_dir, DEFAULT_OUT_DIR, strlen(DEFAULT_OUT_DIR) + 1) != 0)
                                    ? cli_out_dir
                                    : (config.outputDir[0] ? config.outputDir : DEFAULT_OUT_DIR);

    // Initialize setup info (MAC, firmware name, output dir, file extension)
    SetupInfo setup = initializeSetupInfo(final_out_dir, g_reportFormat);
    if (!setup.dirCreated) {
        PRINT_ERROR("Failed to create output directory: %s\n", setup.outputDir);
        for (unsigned j = 0; j < config.whiteListCount; j++)
            if (config.whitelist[j]) free(config.whitelist[j]);
        if (config.whitelist) free(config.whitelist);
        return -1;
    }

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

    PRINT_INFO("* Config loaded successfully [%s]", confFile);
    //printf("\n whitelist=%p\n count=%u\n outDir=%s\n iterations=%u\n interval=%u\n logLevel=%s\n", config.whitelist, config.whiteListCount, config.outputDir, final_iterations, final_interval, config.logLevel);

    if (config.interval > 0 && config.iterations > 0)
    {
        long_run = false; // If both interval and iterations are specified, it's not a long run
    }

    for (int iter = 0; long_run || iter < final_iterations; iter++)
    {
        PRINT_INFO("\n==== Iteration %d%s ====\n", iter + 1, long_run ? "/∞" : "");

        // Get timestamp
        time_t timenow = time(NULL);
        struct tm *tm_info = localtime(&timenow);
        char timestamp[32] = {0};
        char ts[32] = {0};
        strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", tm_info);
        strftime(timestamp, sizeof(timestamp), "%Y%m%d%H%M%S", tm_info);

        // Generate output file name
        char outputFilePath[PATH_MAX * 2] = {0};
        snprintf(outputFilePath, sizeof(outputFilePath), "%s/%s_%s_iter%d_%s",
                 setup.outputDir, setup.mac, timestamp, iter + 1, setup.reportFileName);
        PRINT_INFO("Capturing Process stats into %s\n", outputFilePath);

        /* Recalculate uptime fresh on each iteration */
        const char *uptime = getSystemUptime();

#ifdef ENABLE_CJSON
        if (g_reportFormat == REPORT_JSON) {
            g_rootObject = g_cjson.CreateObject();
            if (!g_rootObject) {
                PRINT_ERROR("Failed to create JSON root object\n");
                for (unsigned j = 0; j < config.whiteListCount; j++)
                    if (config.whitelist[j]) free(config.whitelist[j]);
                if (config.whitelist) free(config.whitelist);
                return -1;
            }
            g_cjson.AddStringToObject(g_rootObject, "firmware_name",  setup.fwName);
            g_cjson.AddStringToObject(g_rootObject, "mac_address",    setup.mac);
            g_cjson.AddStringToObject(g_rootObject, "timestamp",      ts);
            g_cjson.AddStringToObject(g_rootObject, "uptime",         uptime);
            g_cjson.AddStringToObject(g_rootObject, "kernel_version", setup.kernelVersion);
            g_cjson.AddStringToObject(g_rootObject, "report_version", reportVersion);
        }
#endif

        FILE *output = NULL;
        if (g_reportFormat == REPORT_CSV) {
            output = fopen(outputFilePath, "w");
            if (!output) {
                PRINT_ERROR("Error: Failed to open output file '%s' for writing\n", outputFilePath);
                for (unsigned j = 0; j < config.whiteListCount; j++)
                    if (config.whitelist[j]) free(config.whitelist[j]);
                if (config.whitelist) free(config.whitelist);
                return -1;
            }
            fprintf(output, "%s\n", CSV_META_HEADER);
            fprintf(output, "%s,%s,%s,%s,%s,%s\n\n", setup.fwName, setup.mac, ts, uptime, setup.kernelVersion, reportVersion);
            saveMeminfo(output);
            saveFragmentationInfo(output);
            fprintf(output, "\nPID,EXE,RSS,PSS,SHARED_CLEAN,PRIVATE_CLEAN,PRIVATE_DIRTY,SWAP_PSS,"
                            "MIN_FAULTS,MAJ_FAULTS,CPU_TIME\n");
        }

        unsigned long rssTotal = 0, pssTotal = 0, shared_clean_total = 0, private_clean_total = 0, private_dirty_total = 0, swap_pss_total = 0;
        unsigned actualCount = 0;
        if (config.whiteListCount > 0)
        {
            for (unsigned i = 0; i < config.whiteListCount; i++)
            {
                unsigned int pid = 0;
                PRINT_INFO("Processing whitelist item %s\n", config.whitelist[i]);
                if (isPID(config.whitelist[i]))
                {
                    pid = (unsigned int)atoi(config.whitelist[i]);
                }
                else
                {
                    if (!getPIDByProcessName(config.whitelist[i], &pid))
                    {
                        PRINT_ERROR("Process name'%s' not found\n", config.whitelist[i]);
                        continue; // Skip to next item
                    }
                }
                memset(&getProcessInfo, 0, sizeof(getProcessInfo));
                unsigned flags = 0;
                if (!fillProcessStatFields(pid, &getProcessInfo, &flags))
                {
                    PRINT_ERROR("Failed to fill process stat fields for PID %u\n", pid);
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
                    private_clean_total += getProcessInfo.private_clean_total;
                    private_dirty_total += getProcessInfo.private_dirty_total;
                    swap_pss_total += getProcessInfo.swap_pss_total;
                    addProcessInfo(&getProcessInfo);
                    actualCount++;
                }
                else
                {
                    PRINT_ERROR("Failed to get process info for PID %u\n", pid);
                }
            }
            writeProcessInfo(actualCount, output);
            headProcessInfo = NULL; // Reset for next iteration
        }
        else
        {
            PRINT_ERROR("No whitelist specified in config.\n");
            if (output) fclose(output);
#ifdef ENABLE_CJSON
            if (g_reportFormat == REPORT_JSON && g_rootObject) {
                g_cjson.Delete(g_rootObject);
                g_rootObject = NULL;
            }
#endif
            continue;
        }

        if (g_reportFormat == REPORT_CSV) {
            fprintf(output, "0,Total,%lu,%lu,%lu,%lu,%lu,%lu,0,0,0\n",
                    rssTotal, pssTotal, shared_clean_total, private_clean_total,
                    private_dirty_total, swap_pss_total);
            if (g_bwDataAvailable)
                collectBandwidthData(output);
            fclose(output);
        }
#ifdef ENABLE_CJSON
        else if (g_reportFormat == REPORT_JSON) {
            saveMeminfo_JSON(g_rootObject);
            saveFragmentationInfo_JSON(g_rootObject);
            cJSON_t *processesArray = g_cjson.CreateArray();
            if (processesArray) {
                writeProcessInfo_JSON(processesArray);
                g_cjson.AddItemToObject(g_rootObject, "processes", processesArray);
            }
            if (writeJSONToFile(outputFilePath, &setup) != 0) {
                PRINT_ERROR("Failed to write JSON output file: %s\n", outputFilePath);
                for (unsigned j = 0; j < config.whiteListCount; j++)
                    if (config.whitelist[j]) free(config.whitelist[j]);
                if (config.whitelist) free(config.whitelist);
                return -1;
            }
        }
#endif

        if (final_interval > 0 && iter + 1 < final_iterations)
        {
            PRINT_INFO("* %d/%d Completed. Sleeping for %d seconds before next iteration... \n", iter+1, final_iterations, final_interval);
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
    PRINT_INFO("\n---- Completed Data Capture ----\n");
    return 0;
}

/**
 * Reads /proc/meminfo and writes the needed fields and their values to the
 * output file.
 */
void saveMeminfo(FILE *out)
{
    /***
     * MemTotal,MemFree,MemAvailable,Buffers,Cached,SwapCached
     * Active(anon),Inactive(anon),Active(file),Inactive(file)
     * SwapTotal,SwapFree,AnonPages,Mapped,Shmem,Slab,KernelStack,
     * VmallocUsed,CmaTotal,CmaFree
     ***/
#define MEMINFO_NEEDED_FIELDS_COUNT 20
#define MEMINFO_HEADER_TOTAL 256
    static char *meminfoNeeded[MEMINFO_NEEDED_FIELDS_COUNT] = {
        /* Important, MemTotal needs to be at first...other fields doesn't matter */
        "MemTotal","MemFree","MemAvailable","Buffers","Cached","SwapCached",
        "Active(anon)","Inactive(anon)","Active(file)","Inactive(file)",
        "SwapTotal","SwapFree","AnonPages","Mapped","Shmem","Slab","KernelStack",
        "VmallocUsed","CmaFree","CmaTotal"};
    static char meminfoHeader[MEMINFO_HEADER_TOTAL] = {0};
    static char meminfoValue[MEMINFO_HEADER_TOTAL] = {0};
    static int skipMemTotal = 0;
    static int skipArray[MEMINFO_NEEDED_FIELDS_COUNT] = {0};
    static int learnt = 0;

#ifdef TESTME
    char tstmeminfoHeader[MEMINFO_HEADER_TOTAL] = {0};
    char tstmeminfoValue[MEMINFO_HEADER_TOTAL] = {0};
    int tstprocessHeaderIndex = 0;
    int tstprocessValueIndex = 0;
    FILE *meminfo = fopen((isTestMode)?testMeminfo:MEMINFO_FILE, "r");
#else
    FILE *meminfo = fopen(MEMINFO_FILE, "r");
#endif
    if (meminfo) {
        unsigned long value;
        char tmp[128];
        int skipCount = skipArray[1];
        int processIndex = learnt;
        int processValIndex = skipMemTotal;
        while (fgets(tmp, 127, meminfo) && (processIndex < MEMINFO_NEEDED_FIELDS_COUNT)) {
#ifdef TESTME
            char tstname[64];
            if (!sscanf(tmp, "%s %lu kB", tstname, &value)) {
                PRINT_ERROR("%s: Error parsing [%s]\n", __FUNCTION__, tmp);
                continue; 
            }
            tstname[strlen(tstname)-1] = '\0';
            for (int tsti=0; tsti<MEMINFO_NEEDED_FIELDS_COUNT; tsti++) {
                if (!strcmp(tstname, meminfoNeeded[tsti])) {
                    if (!tstprocessHeaderIndex) {
                        tstprocessHeaderIndex += sprintf(tstmeminfoHeader+tstprocessHeaderIndex, "%s", meminfoNeeded[tsti]);
                        tstprocessValueIndex += sprintf(tstmeminfoValue+tstprocessValueIndex, "%lu", value);
                    }
                    else {
                        tstprocessHeaderIndex += sprintf(tstmeminfoHeader+tstprocessHeaderIndex, ",%s", meminfoNeeded[tsti]);
                        tstprocessValueIndex += sprintf(tstmeminfoValue+tstprocessValueIndex, ",%lu", value);
                    }
                }
            }
#endif
            if (learnt) {
                if (! --skipCount) {
                    if (!sscanf(tmp, "%*s %lu kB", &value)) {
                        PRINT_ERROR("%s: Error parsing [%s]\n", __FUNCTION__, tmp);
                        continue; 
                    }
                    skipCount = skipArray[++processIndex];
                    processValIndex += sprintf(meminfoValue+processValIndex, ",%lu", value);
                }
            }
            else 
            {
                char name[64];
                // Below lines repeat, but okay..for clarity and not needed to check whether learnt again
                if (!sscanf(tmp, "%s %lu kB", name, &value)) {
                    PRINT_ERROR("%s: Error parsing [%s]\n", __FUNCTION__, tmp);
                    continue; 
                }
                skipCount++;
                name[strlen(name)-1] = '\0';
                if (!strcmp(name, meminfoNeeded[processIndex])) {
                    PRINT_DBG_INITIAL("Found %s storing at index %d\n", name, processIndex);
                    if (processIndex) { // For MemTotal, skip always since we've the constant value
                        skipArray[processIndex++] = skipCount;
                        skipCount = 0;
                        processValIndex += sprintf(meminfoValue+processValIndex, ",%lu", value);
                    }
                    else {
                        processIndex++;
                        processValIndex += sprintf(meminfoValue+processValIndex, "%lu", value);
                        skipMemTotal = processValIndex;
                    }
                    }
                else {
                    // If the list is in order, then we can skip and go on. 
                    // But if for some reason, the order is not maintained, then 
                    // do the loop of meminfoNeeded and figure out..
                    for (int i=processIndex+1; i<MEMINFO_NEEDED_FIELDS_COUNT; i++)
                    {
                        if (!strcmp(name, meminfoNeeded[i])) {
                            PRINT_DBG_INITIAL("Found by looping %s storing at index %d\n", name, processIndex);
                            // Let's swap the entries in meminfoNeeded 
                            char *tmpp = meminfoNeeded[processIndex];
                            meminfoNeeded[processIndex] = meminfoNeeded[i];
                            meminfoNeeded[i] = tmpp;
                            skipArray[processIndex++] = skipCount;
                            skipCount = 0;
                            processValIndex += sprintf(meminfoValue+processValIndex, ",%lu", value);
                            break;
                        }
                    }
                }
            }
        } // while (fgets(tmp, 127,...
        fclose(meminfo);        
        if (!learnt) {
            PRINT_DBG_INITIAL("Total %d found %d\n", MEMINFO_NEEDED_FIELDS_COUNT, processIndex);
            learnt = 1;
            int index = 0;
            for (int i=0; i < processIndex; i++) {
                if ((MEMINFO_HEADER_TOTAL - 16) < index) {
                    PRINT_MUST("Buffer insufficient....\n");
                    exit(0);
                }
                PRINT_DBG_INITIAL("index %d at %d..meminfo [%s] skip [%d]\n", index,i,meminfoNeeded[i],skipArray[i]);
                if (index) {
                    index += sprintf(meminfoHeader+index, ",%s", meminfoNeeded[i]);
                }
                else {
                    index += sprintf(meminfoHeader+index, "%s", meminfoNeeded[i]);
                }
            }
        }
        fprintf(out, "%s:\n%s", MEMINFO_FILE, meminfoHeader);
        fprintf(out, "\n%s\n", meminfoValue);
#ifdef TESTME
        if (strcmp(meminfoHeader, tstmeminfoHeader) || strcmp(meminfoValue, tstmeminfoValue)) {
            PRINT_ERROR("Test Failed..meminfoHeader vs tstmeminfoHeader [%s] vs [%s]\n", meminfoHeader, tstmeminfoHeader);
            PRINT_ERROR("meminfoValue vs tstmeminfoValue [%s] vs [%s]\n", meminfoValue, tstmeminfoValue);
            unitTestFailed = 1;
        }
#endif
    }
    else {
        PRINT_MUST("Error opening %s, [%s]\n", MEMINFO_FILE, strerror(errno));
    }
}

// -----------------------------
// JSON Output Functions
// -----------------------------

#ifdef ENABLE_CJSON

/**
 * @brief Add meminfo key/value fields to a JSON root object.
 *
 * Reads /proc/meminfo (or the test fixture when TESTME is set) and
 * writes each needed field as a numeric member of @p root.
 * Uses the same field list as the CSV saveMeminfo() for consistency.
 */
void saveMeminfo_JSON(cJSON_t *root)
{
    static const char *meminfoNeeded[] = {
        "MemTotal", "MemFree", "MemAvailable", "Buffers", "Cached", "SwapCached",
        "Active(anon)", "Inactive(anon)", "Active(file)", "Inactive(file)",
        "SwapTotal", "SwapFree", "AnonPages", "Mapped", "Shmem", "Slab",
        "KernelStack", "VmallocUsed", "CmaFree", "CmaTotal"
    };
    const int fieldCount = (int)(sizeof(meminfoNeeded) / sizeof(meminfoNeeded[0]));

    cJSON_t *meminfoObj = g_cjson.CreateObject();
    if (!meminfoObj) {
        PRINT_ERROR("Failed to create meminfo JSON object\n");
        return;
    }

#ifdef TESTME
    FILE *meminfo = fopen((isTestMode) ? testMeminfo : MEMINFO_FILE, "r");
#else
    FILE *meminfo = fopen(MEMINFO_FILE, "r");
#endif
    if (!meminfo) {
        PRINT_ERROR("Error opening %s: %s\n", MEMINFO_FILE, strerror(errno));
        g_cjson.Delete(meminfoObj);
        return;
    }

    char tmp[128];
    char name[64];
    unsigned long value;

    while (fgets(tmp, sizeof(tmp), meminfo)) {
        if (sscanf(tmp, "%63s %lu kB", name, &value) == 2) {
            size_t len = strlen(name);
            if (len > 0 && name[len - 1] == ':')
                name[len - 1] = '\0';
            for (int i = 0; i < fieldCount; i++) {
                if (strcmp(name, meminfoNeeded[i]) == 0) {
                    g_cjson.AddNumberToObject(meminfoObj, name, (double)value);
                    break;
                }
            }
        }
    }
    fclose(meminfo);
    g_cjson.AddItemToObject(root, "meminfo", meminfoObj);
}

/**
 * @brief Write process linked-list to a JSON array, then free each node.
 *
 * Mirrors writeProcessInfo() for CSV. Accumulates totals and appends a
 * synthetic "Total" entry as the last element of @p processesArray.
 */
void writeProcessInfo_JSON(cJSON_t *processesArray)
{
    unsigned long rssTotal = 0, pssTotal = 0;
    unsigned long shared_clean_total = 0, private_clean_total = 0;
    unsigned long private_dirty_total = 0, swap_pss_total = 0;

    Process_Info *cur = headProcessInfo;
    Process_Info *tofree;

    while (cur) {
        cJSON_t *procObj = g_cjson.CreateObject();
        if (procObj) {
            g_cjson.AddNumberToObject(procObj, "pid",           (double)cur->pid);
            g_cjson.AddStringToObject(procObj, "name",          cur->name);
            g_cjson.AddNumberToObject(procObj, "rss",           (double)cur->rssTotal);
            g_cjson.AddNumberToObject(procObj, "pss",           (double)cur->pssTotal);
            g_cjson.AddNumberToObject(procObj, "shared_clean",  (double)cur->shared_clean_total);
            g_cjson.AddNumberToObject(procObj, "private_clean", (double)cur->private_clean_total);
            g_cjson.AddNumberToObject(procObj, "private_dirty", (double)cur->private_dirty_total);
            g_cjson.AddNumberToObject(procObj, "swap_pss",      (double)cur->swap_pss_total);
            g_cjson.AddNumberToObject(procObj, "min_faults",    (double)cur->minFaults);
            g_cjson.AddNumberToObject(procObj, "maj_faults",    (double)cur->majFaults);
            g_cjson.AddNumberToObject(procObj, "cpu_time",      (double)cur->cputime);
            g_cjson.AddItemToArray(processesArray, procObj);
        }
        rssTotal           += cur->rssTotal;
        pssTotal           += cur->pssTotal;
        shared_clean_total += cur->shared_clean_total;
        private_clean_total+= cur->private_clean_total;
        private_dirty_total+= cur->private_dirty_total;
        swap_pss_total     += cur->swap_pss_total;

        tofree = cur;
        cur    = cur->next;
        free(tofree);
    }
    headProcessInfo = NULL;

    // Append a synthetic totals row (mirrors the CSV "0,Total,..." line)
    cJSON_t *totalObj = g_cjson.CreateObject();
    if (totalObj) {
        g_cjson.AddNumberToObject(totalObj, "pid",           0);
        g_cjson.AddStringToObject(totalObj, "name",          "Total");
        g_cjson.AddNumberToObject(totalObj, "rss",           (double)rssTotal);
        g_cjson.AddNumberToObject(totalObj, "pss",           (double)pssTotal);
        g_cjson.AddNumberToObject(totalObj, "shared_clean",  (double)shared_clean_total);
        g_cjson.AddNumberToObject(totalObj, "private_clean", (double)private_clean_total);
        g_cjson.AddNumberToObject(totalObj, "private_dirty", (double)private_dirty_total);
        g_cjson.AddNumberToObject(totalObj, "swap_pss",      (double)swap_pss_total);
        g_cjson.AddNumberToObject(totalObj, "min_faults",    0);
        g_cjson.AddNumberToObject(totalObj, "maj_faults",    0);
        g_cjson.AddNumberToObject(totalObj, "cpu_time",      0);
        g_cjson.AddItemToArray(processesArray, totalObj);
    }
}

/**
 * @brief Serialise g_rootObject to @p filepath and free the JSON tree.
 *
 * Uses cJSON_Print (pretty) or cJSON_PrintUnformatted depending on the
 * g_jsonPrettyPrint flag.  Always cleans up g_rootObject regardless of
 * success or failure.
 *
 * @return 0 on success, -1 on any error.
 */
int writeJSONToFile(const char *filepath, const SetupInfo *setup)
{
    (void)setup; /* metadata already written into g_rootObject at creation */

    if (!g_rootObject) {
        PRINT_ERROR("No JSON data to write\n");
        return -1;
    }

    FILE *out = fopen(filepath, "w");
    if (!out) {
        PRINT_ERROR("Failed to open %s for writing: %s\n", filepath, strerror(errno));
        g_cjson.Delete(g_rootObject);
        g_rootObject = NULL;
        return -1;
    }

    char *jsonStr = g_jsonPrettyPrint
                    ? g_cjson.Print(g_rootObject)
                    : g_cjson.PrintUnformatted(g_rootObject);

    int rc = 0;
    if (jsonStr) {
        if (fprintf(out, "%s\n", jsonStr) < 0) {
            PRINT_ERROR("Failed to write JSON to %s: %s\n", filepath, strerror(errno));
            rc = -1;
        }
        free(jsonStr);
    } else {
        PRINT_ERROR("cJSON serialisation returned NULL for %s\n", filepath);
        rc = -1;
    }

    fclose(out);
    g_cjson.Delete(g_rootObject);
    g_rootObject = NULL;
    return rc;
}

#endif /* ENABLE_CJSON */

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
    bool isSystemWide = true;
    bool long_run = true;
    char confFile[PATH_MAX] = {0};
    char out_dir[PATH_MAX] = DEFAULT_OUT_DIR;
    int cli_iterations = -1;
    int cli_interval = -1;

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
                    PRINT_ERROR("Error: Config file '%s' does not exist or cannot be opened.\n", confFile);
                    printHelpAndUsage(argv, true, 1);
                }
                fclose(fp);
                isSystemWide = false; // Config mode implies not system-wide
                isConfigPresent = true;
                i++; // Skip next arg (conf file)
            }
            else
            {
                PRINT_ERROR("Error: Missing config file path after %s\n", argv[i]);
                printHelpAndUsage(argv, false, 1);
            }
        }
        else if (!strncmp(argv[i], "-a", 3) || !strncmp(argv[i], "--all", 6))
        { // include kernel threads
            enableKThreads = true;
        }
        else if (!strncmp(argv[i], "-h", 3) || !strncmp(argv[i], "--help", 7))
        { // help
            printHelpAndUsage(argv, true, 0);
        }
#ifdef TESTME
        else if (!strncmp(argv[i], "-t", 3) || !strncmp(argv[i], "--test", 7))
        { // test mode
            isTestMode = 2;
            if ((i+2) < argc) {
                i++;
                FILE *testMapFd = fopen(argv[i], "r");
                if (testMapFd) {
                    fclose(testMapFd);
                    strncpy(testSmap, argv[i], 128);
                    i++;
                    testMapFd = fopen(argv[i], "r");
                    if (testMapFd) {
                        fclose(testMapFd);
                        strncpy(testMeminfo, argv[i], 128);

                        if ((i + 1) < argc && argv[i + 1][0] != '-') {
                            i++;
                            testMapFd = fopen(argv[i], "r");
                            if (testMapFd) {
                                fclose(testMapFd);
                                strncpy(testBuddyinfo, argv[i], 128);
                            } else {
                                PRINT_ERROR("Test buddyinfo file %s open error %d [%s]\n", argv[i], errno, strerror(errno));
                                printHelpAndUsage(argv, false, 1);
                            }
                        }

                        if ((i + 1) < argc && argv[i + 1][0] != '-') {
                            i++;
                            testMapFd = fopen(argv[i], "r");
                            if (testMapFd) {
                                fclose(testMapFd);
                                strncpy(testPagetypeinfo, argv[i], 128);
                            } else {
                                PRINT_ERROR("Test pagetypeinfo file %s open error %d [%s]\n", argv[i], errno, strerror(errno));
                                printHelpAndUsage(argv, false, 1);
                            }
                        }

                        continue;
                    }
                    else {
                        PRINT_ERROR("Test meminfo file %s open error %d [%s]\n", argv[i], errno, strerror(errno));
                    }
                } else {
                    PRINT_ERROR("Test map file %s open error %d [%s]\n", argv[i], errno, strerror(errno));
		        }
            }
            printHelpAndUsage(argv, false, 1);
        }
#endif
        else if (!strncmp(argv[i], "-o", 3) || !strncmp(argv[i], "--output", 9))
        { // output directory
            if (i + 1 < argc)
            {
                strncpy(out_dir, argv[i + 1], PATH_MAX - 1);
                i++; // skip next arg (output directory)
            }
            else
            {
                PRINT_ERROR("Error: Missing output directory path after %s\n", argv[i]);
                printHelpAndUsage(argv, false, 1);
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
                PRINT_ERROR("Error: Missing interval value after %s\n", argv[i]);
                printHelpAndUsage(argv, false, 1);
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
                PRINT_ERROR("Error: Missing iterations value after %s\n", argv[i]);
                printHelpAndUsage(argv, false, 1);
            }
        }
        else if (!strncmp(argv[i], "-s", 3) || !strncmp(argv[i], "--smaps", 8))
        {
            force_smaps = 1;
        }
        else if (!strncmp(argv[i], "--fmt", 5))
        { // output format: csv or json
            if (i + 1 < argc)
            {
                i++;
                if (!strncmp(argv[i], "json", 5))
                {
#ifdef ENABLE_CJSON
                    g_reportFormat = REPORT_JSON;
#else
                    printf("Warning: --fmt json requested but cJSON support not compiled in.\n");
                    printf("         Build with --enable-cjson flag. Falling back to CSV.\n");
                    g_reportFormat = REPORT_CSV;
#endif
                }
                else if (!strncmp(argv[i], "csv", 4))
                {
                    g_reportFormat = REPORT_CSV;
                }
                else
                {
                    PRINT_MUST("Error: Unsupported format '%s'. Supported: csv, json.\n", argv[i]);
                    printHelpAndUsage(argv, false, 1);
                }
            }
            else
            {
                PRINT_MUST("Error: Missing format value after %s\n", argv[i]);
                printHelpAndUsage(argv, false, 1);
            }
        }
        else if (!strncmp(argv[i], "--json-pretty", 13))
        {
#ifdef ENABLE_CJSON
            g_jsonPrettyPrint = true;
#else
            printf("Warning: --json-pretty ignored (cJSON support not compiled in).\n");
#endif
        }
        else
        {
            PRINT_ERROR("Error: Unrecognized argument '%s'\n", argv[i]);
            printHelpAndUsage(argv, false, 1);
        }
    }

    printf("\nExecuting: ");
    for (int i = 0; i < argc; i++)
    {
        printf("%s ", argv[i]);
    }
    printf("\n");

    updateBandwidthAvailability();

    if (force_smaps)
    {
        smaps_rollup = 0;
        strcpy(gSMAPS_OR_ROLLUP, "smaps");
        getProcessInfos_ptr = getProcessInfos_initial;
    }
    else
    {
        smaps_rollup = 1;
        strcpy(gSMAPS_OR_ROLLUP, "smaps_rollup");
        getProcessInfos_ptr = getProcessInfos_rollup;
    }

#ifdef ENABLE_CJSON
    if (g_reportFormat == REPORT_JSON) {
        if (loadCjson() != 0) {
            /* loadCjson() already set g_reportFormat = REPORT_CSV and printed the reason */
            PRINT_MUST("JSON: Continuing with CSV fallback.\n");
        }
    }
#endif

    if (isConfigPresent)
    {
#ifdef TESTME
        return (handleConfigMode(confFile, out_dir, cli_iterations, cli_interval, enableKThreads, long_run) == 0) ? 0 : 1;
#else
        handleConfigMode(confFile, out_dir, cli_iterations, cli_interval, enableKThreads, long_run);
#endif
    }
    else if (isSystemWide)
    {
        int final_interval = 0;
        int final_iterations = 1;
        if (long_run) {
            final_interval = LONG_RUN_INTERVAL;
        final_iterations = LONG_RUN_ITERATIONS;
        }
        else {
            final_interval = (0 < cli_interval)? cli_interval : DEFAULT_INTERVAL;
            final_iterations = (0 < cli_iterations) ? cli_iterations : DEFAULT_ITERATIONS;
        }
        PRINT_MUST("* Running %d iterations with %ds interval (indefinitely?: %s)\n", final_iterations, final_interval, long_run ? "yes" : "no");
#ifdef TESTME
        return (collectSystemMemoryStats(enableKThreads, out_dir, final_iterations, final_interval, long_run) == 0) ? 0 : 1;
#else
        collectSystemMemoryStats(enableKThreads, out_dir, final_iterations, final_interval, long_run);
#endif
    }

#ifdef ENABLE_CJSON
    unloadCjson();
#endif
    return 0;
}
