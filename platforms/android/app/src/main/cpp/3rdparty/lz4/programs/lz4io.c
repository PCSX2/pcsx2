/*
  LZ4io.c - LZ4 File/Stream Interface
  Copyright (c) Yann Collet. All rights reserved.

  GPL v2 License

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License along
  with this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

  You can contact the author at :
  - LZ4 source repository : https://github.com/lz4/lz4
  - LZ4 public forum : https://groups.google.com/forum/#!forum/lz4c
*/
/*
  Note : this is stand-alone program.
  It is not part of LZ4 compression library, it is a user code of the LZ4 library.
  - The license of LZ4 library is BSD.
  - The license of xxHash library is BSD.
  - The license of this source file is GPLv2.
*/


/*-************************************
*  Compiler options
**************************************/
#ifdef _MSC_VER    /* Visual Studio */
#  pragma warning(disable : 4127)    /* disable: C4127: conditional expression is constant */
#endif
#if defined(__MINGW32__) && !defined(_POSIX_SOURCE)
#  define _POSIX_SOURCE 1          /* disable %llu warnings with MinGW on Windows */
#endif


/*****************************
*  Includes
*****************************/
#include "platform.h"  /* Large File Support, SET_BINARY_MODE, SET_SPARSE_FILE_MODE, PLATFORM_POSIX_VERSION, __64BIT__ */
#include "timefn.h"    /* TIME_ */
#include "util.h"      /* UTIL_getFileStat, UTIL_setFileStat */
#include <stdio.h>     /* fprintf, fopen, fread, stdin, stdout, fflush, getchar */
#include <stdlib.h>    /* malloc, free */
#include <string.h>    /* strerror, strcmp, strlen */
#include <time.h>      /* clock_t, for cpu-time */
#include <sys/types.h> /* stat64 */
#include <sys/stat.h>  /* stat64 */
#include "lz4conf.h"   /* compile-time constants */
#include "lz4io.h"
#include "lz4.h"       /* required for legacy format */
#include "lz4hc.h"     /* required for legacy format */
#define LZ4F_STATIC_LINKING_ONLY
#include "lz4frame.h"  /* LZ4F_* */
#include "xxhash.h"    /* frame checksum (MT mode) */


/*****************************
*  Constants
*****************************/
#define KB *(1 <<10)
#define MB *(1 <<20)
#define GB *(1U<<30)

#define _1BIT  0x01
#define _2BITS 0x03
#define _3BITS 0x07
#define _4BITS 0x0F
#define _8BITS 0xFF

#define MAGICNUMBER_SIZE    4
#define LZ4IO_MAGICNUMBER   0x184D2204
#define LZ4IO_SKIPPABLE0    0x184D2A50
#define LZ4IO_SKIPPABLEMASK 0xFFFFFFF0
#define LEGACY_MAGICNUMBER  0x184C2102

#define CACHELINE 64
#define LEGACY_BLOCKSIZE   (8 MB)
#define MIN_STREAM_BUFSIZE (192 KB)
#define LZ4IO_BLOCKSIZEID_DEFAULT 7
#define LZ4_MAX_DICT_SIZE (64 KB)

#undef MIN
#define MIN(a,b)  ((a)<(b)?(a):(b))

/**************************************
*  Time and Display
**************************************/
#define DISPLAY(...)         fprintf(stderr, __VA_ARGS__)
#define DISPLAYOUT(...)      fprintf(stdout, __VA_ARGS__)
#define DISPLAYLEVEL(l, ...) if (g_displayLevel>=l) { DISPLAY(__VA_ARGS__); if (g_displayLevel>=4) fflush(stderr); }
static int g_displayLevel = 0;   /* 0 : no display  ; 1: errors  ; 2 : + result + interaction + warnings ; 3 : + progression; 4 : + information */

#define DISPLAYUPDATE(l, ...) if (g_displayLevel>=l) { \
            if ( (TIME_clockSpan_ns(g_time) > refreshRate)  \
              || (g_displayLevel>=4) ) {               \
                g_time = TIME_getTime();               \
                DISPLAY(__VA_ARGS__);                  \
                if (g_displayLevel>=4) fflush(stderr); \
        }   }
static const Duration_ns refreshRate = 200000000;
static TIME_t g_time = { 0 };

/* Gives the cpu time,
 * i.e. the sum of kernel time + user time spent in all active threads since the beginning of the program.
 * When compared with time duration, it gives a sense of cpu load.
 * On Windows: uses GetProcessTimes() counters.
 * On posix: relies on the fact that `clock()` measures cpu time.
 */
static double cpuTime_sec(void)
{
#ifdef _WIN32
    FILETIME creationTime, exitTime, kernelTime, userTime;
    GetProcessTimes(GetCurrentProcess(), &creationTime, &exitTime, &kernelTime, &userTime);
    return ( ((double)kernelTime.dwLowDateTime + (double)userTime.dwLowDateTime)
           + ((double)kernelTime.dwHighDateTime + (double)userTime.dwHighDateTime) * (double)(1ULL << 32) )
           * 100. / 1000000000.;
#else
    return (double)clock() / (double)CLOCKS_PER_SEC;
#endif
}

static double cpuTimeSpan_sec(double startTime)
{
    return cpuTime_sec() - startTime;
}

static void LZ4IO_finalTimeDisplay(TIME_t timeStart, double cpuTimeStart, unsigned long long size)
{
#if LZ4IO_MULTITHREAD
    if (!TIME_support_MT_measurements()) {
        DISPLAYLEVEL(5, "time measurements not compatible with multithreading \n");
    } else
#endif
    {
        Duration_ns const duration_ns = TIME_clockSpan_ns(timeStart);
        double const seconds = (double)(duration_ns + !duration_ns) / (double)1000000000.;
        double const cpuLoad_s = cpuTimeSpan_sec(cpuTimeStart);
        DISPLAYLEVEL(3,"Done in %.2f s ==> %.2f MiB/s  (cpu load : %.0f%%)\n", seconds,
                        (double)size / seconds / 1024. / 1024.,
                        (cpuLoad_s / seconds) * 100.);
    }
}

/**************************************
*  Exceptions
***************************************/
#ifndef DEBUG
#  define DEBUG 0
#endif
#define DEBUGOUTPUT(...) if (DEBUG) DISPLAY(__VA_ARGS__);
#define END_PROCESS(error, ...)                                   \
{                                                                 \
    DEBUGOUTPUT("Error in %s, line %i : \n", __FILE__, __LINE__); \
    DISPLAYLEVEL(1, "Error %i : ", error);                        \
    DISPLAYLEVEL(1, __VA_ARGS__);                                 \
    DISPLAYLEVEL(1, " \n");                                       \
    fflush(NULL);                                                 \
    exit(error);                                                  \
}

#define LZ4IO_STATIC_ASSERT(c)   { enum { LZ4IO_static_assert = 1/(int)(!!(c)) }; }   /* use after variable declarations */


/* ************************************************** */
/* ****************** Init functions ******************** */
/* ************************************************** */

int LZ4IO_defaultNbWorkers(void)
{
#if LZ4IO_MULTITHREAD
    int const nbCores = UTIL_countCores();
    int const spared = 1 + ((unsigned)nbCores >> 3);
    if (nbCores <= spared) return 1;
    return nbCores - spared;
#else
    return 1;
#endif
}

/* ************************************************** */
/* ****************** Parameters ******************** */
/* ************************************************** */

struct LZ4IO_prefs_s {
    int passThrough;
    int overwrite;
    int testMode;
    int blockSizeId;
    size_t blockSize;
    int blockChecksum;
    int streamChecksum;
    int blockIndependence;
    int sparseFileSupport;
    int contentSizeFlag;
    int useDictionary;
    unsigned favorDecSpeed;
    const char* dictionaryFilename;
    int removeSrcFile;
    int nbWorkers;
};

void LZ4IO_freePreferences(LZ4IO_prefs_t* prefs)
{
    free(prefs);
}

LZ4IO_prefs_t* LZ4IO_defaultPreferences(void)
{
    LZ4IO_prefs_t* const prefs = (LZ4IO_prefs_t*)malloc(sizeof(*prefs));
    if (!prefs) END_PROCESS(11, "Can't even allocate LZ4IO preferences");
    prefs->passThrough = 0;
    prefs->overwrite = 1;
    prefs->testMode = 0;
    prefs->blockSizeId = LZ4IO_BLOCKSIZEID_DEFAULT;
    prefs->blockSize = 0;
    prefs->blockChecksum = 0;
    prefs->streamChecksum = 1;
    prefs->blockIndependence = 1;
    prefs->sparseFileSupport = 1;
    prefs->contentSizeFlag = 0;
    prefs->useDictionary = 0;
    prefs->favorDecSpeed = 0;
    prefs->dictionaryFilename = NULL;
    prefs->removeSrcFile = 0;
    prefs->nbWorkers = LZ4IO_defaultNbWorkers();
    return prefs;
}

int LZ4IO_setNbWorkers(LZ4IO_prefs_t* const prefs, int nbWorkers)
{
    if (nbWorkers < 1 ) nbWorkers = 1;
    nbWorkers = MIN(nbWorkers, LZ4_NBWORKERS_MAX);
    prefs->nbWorkers = nbWorkers;
    return nbWorkers;
}

int LZ4IO_setDictionaryFilename(LZ4IO_prefs_t* const prefs, const char* dictionaryFilename)
{
    prefs->dictionaryFilename = dictionaryFilename;
    prefs->useDictionary = dictionaryFilename != NULL;
    return prefs->useDictionary;
}

/* Default setting : passThrough = 0; return : passThrough mode (0/1) */
int LZ4IO_setPassThrough(LZ4IO_prefs_t* const prefs, int yes)
{
   prefs->passThrough = (yes!=0);
   return prefs->passThrough;
}

/* Default setting : overwrite = 1; return : overwrite mode (0/1) */
int LZ4IO_setOverwrite(LZ4IO_prefs_t* const prefs, int yes)
{
   prefs->overwrite = (yes!=0);
   return prefs->overwrite;
}

/* Default setting : testMode = 0; return : testMode (0/1) */
int LZ4IO_setTestMode(LZ4IO_prefs_t* const prefs, int yes)
{
   prefs->testMode = (yes!=0);
   return prefs->testMode;
}

/* blockSizeID : valid values : 4-5-6-7 */
size_t LZ4IO_setBlockSizeID(LZ4IO_prefs_t* const prefs, unsigned bsid)
{
    static const size_t blockSizeTable[] = { 64 KB, 256 KB, 1 MB, 4 MB };
    static const unsigned minBlockSizeID = 4;
    static const unsigned maxBlockSizeID = 7;
    if ((bsid < minBlockSizeID) || (bsid > maxBlockSizeID)) return 0;
    prefs->blockSizeId = (int)bsid;
    prefs->blockSize = blockSizeTable[(unsigned)prefs->blockSizeId-minBlockSizeID];
    return prefs->blockSize;
}

size_t LZ4IO_setBlockSize(LZ4IO_prefs_t* const prefs, size_t blockSize)
{
    static const size_t minBlockSize = 32;
    static const size_t maxBlockSize = 4 MB;
    unsigned bsid = 0;
    if (blockSize < minBlockSize) blockSize = minBlockSize;
    if (blockSize > maxBlockSize) blockSize = maxBlockSize;
    prefs->blockSize = blockSize;
    blockSize--;
    /* find which of { 64k, 256k, 1MB, 4MB } is closest to blockSize */
    while (blockSize >>= 2)
        bsid++;
    if (bsid < 7) bsid = 7;
    prefs->blockSizeId = (int)(bsid-3);
    return prefs->blockSize;
}

/* Default setting : 1 == independent blocks */
int LZ4IO_setBlockMode(LZ4IO_prefs_t* const prefs, LZ4IO_blockMode_t blockMode)
{
    prefs->blockIndependence = (blockMode == LZ4IO_blockIndependent);
    return prefs->blockIndependence;
}

/* Default setting : 0 == no block checksum */
int LZ4IO_setBlockChecksumMode(LZ4IO_prefs_t* const prefs, int enable)
{
    prefs->blockChecksum = (enable != 0);
    return prefs->blockChecksum;
}

/* Default setting : 1 == checksum enabled */
int LZ4IO_setStreamChecksumMode(LZ4IO_prefs_t* const prefs, int enable)
{
    prefs->streamChecksum = (enable != 0);
    return prefs->streamChecksum;
}

/* Default setting : 0 (no notification) */
int LZ4IO_setNotificationLevel(int level)
{
    g_displayLevel = level;
    return g_displayLevel;
}

/* Default setting : 1 (auto: enabled on file, disabled on stdout) */
int LZ4IO_setSparseFile(LZ4IO_prefs_t* const prefs, int enable)
{
    prefs->sparseFileSupport = 2*(enable!=0);  /* 2==force enable */
    return prefs->sparseFileSupport;
}

/* Default setting : 0 (disabled) */
int LZ4IO_setContentSize(LZ4IO_prefs_t* const prefs, int enable)
{
    prefs->contentSizeFlag = (enable!=0);
    return prefs->contentSizeFlag;
}

/* Default setting : 0 (disabled) */
void LZ4IO_favorDecSpeed(LZ4IO_prefs_t* const prefs, int favor)
{
    prefs->favorDecSpeed = (favor!=0);
}

void LZ4IO_setRemoveSrcFile(LZ4IO_prefs_t* const prefs, unsigned flag)
{
  prefs->removeSrcFile = (flag>0);
}


/* ************************************************************************ **
** ********************** String functions ********************* **
** ************************************************************************ */

static int LZ4IO_isDevNull(const char* s)
{
    return UTIL_sameString(s, nulmark);
}

static int LZ4IO_isStdin(const char* s)
{
    return UTIL_sameString(s, stdinmark);
}

static int LZ4IO_isStdout(const char* s)
{
    return UTIL_sameString(s, stdoutmark);
}


/* ************************************************************************ **
** ********************** LZ4 File / Pipe compression ********************* **
** ************************************************************************ */

static int LZ4IO_isSkippableMagicNumber(unsigned int magic) {
    return (magic & LZ4IO_SKIPPABLEMASK) == LZ4IO_SKIPPABLE0;
}


/** LZ4IO_openSrcFile() :
 * condition : `srcFileName` must be non-NULL.
 * @result : FILE* to `dstFileName`, or NULL if it fails */
static FILE* LZ4IO_openSrcFile(const char* srcFileName)
{
    FILE* f;

    if (LZ4IO_isStdin(srcFileName)) {
        DISPLAYLEVEL(4,"Using stdin for input \n");
        f = stdin;
        SET_BINARY_MODE(stdin);
        return f;
    }

    if (UTIL_isDirectory(srcFileName)) {
        DISPLAYLEVEL(1, "lz4: %s is a directory -- ignored \n", srcFileName);
        return NULL;
    }

    f = fopen(srcFileName, "rb");
    if (f==NULL) DISPLAYLEVEL(1, "%s: %s \n", srcFileName, strerror(errno));
    return f;
}

/** FIO_openDstFile() :
 *  prefs is writable, because sparseFileSupport might be updated.
 *  condition : `dstFileName` must be non-NULL.
 * @result : FILE* to `dstFileName`, or NULL if it fails */
static FILE*
LZ4IO_openDstFile(const char* dstFileName, const LZ4IO_prefs_t* const prefs)
{
    FILE* f;
    assert(dstFileName != NULL);

    if (LZ4IO_isStdout(dstFileName)) {
        DISPLAYLEVEL(4, "Using stdout for output \n");
        f = stdout;
        SET_BINARY_MODE(stdout);
        if (prefs->sparseFileSupport==1) {
            DISPLAYLEVEL(4, "Sparse File Support automatically disabled on stdout ;"
                            " to force-enable it, add --sparse command \n");
        }
    } else {
        if (!prefs->overwrite && !LZ4IO_isDevNull(dstFileName)) {
            /* Check if destination file already exists */
            FILE* const testf = fopen( dstFileName, "rb" );
            if (testf != NULL) {  /* dest exists, prompt for overwrite authorization */
                fclose(testf);
                if (g_displayLevel <= 1) {  /* No interaction possible */
                    DISPLAY("%s already exists; not overwritten  \n", dstFileName);
                    return NULL;
                }
                DISPLAY("%s already exists; do you want to overwrite (y/N) ? ", dstFileName);
                {   int ch = getchar();
                    if ((ch!='Y') && (ch!='y')) {
                        DISPLAY("    not overwritten  \n");
                        return NULL;
                    }
                    while ((ch!=EOF) && (ch!='\n')) ch = getchar();  /* flush rest of input line */
        }   }   }
        f = fopen( dstFileName, "wb" );
        if (f==NULL) DISPLAYLEVEL(1, "%s: %s\n", dstFileName, strerror(errno));
    }

    /* sparse file */
    {   int const sparseMode = (prefs->sparseFileSupport - (f==stdout)) > 0;
        if (f && sparseMode) { SET_SPARSE_FILE_MODE(f); }
    }

    return f;
}


/***************************************
*   MT I/O
***************************************/

#include "threadpool.h"

typedef struct {
    void* buf;
    size_t size;
    unsigned long long rank;
} BufferDesc;

typedef struct {
    unsigned long long expectedRank;
    BufferDesc* buffers;
    size_t capacity;
    size_t blockSize;
    unsigned long long totalCSize;
} WriteRegister;

static void WR_destroy(WriteRegister* wr)
{
    free(wr->buffers);
}

#define WR_INITIAL_BUFFER_POOL_SIZE 16
/* Note: WR_init() can fail (allocation)
 * check that wr->buffers!= NULL for success */
static WriteRegister WR_init(size_t blockSize)
{
    WriteRegister wr = { 0, NULL, WR_INITIAL_BUFFER_POOL_SIZE, 0, 0 };
    wr.buffers = (BufferDesc*)calloc(1, WR_INITIAL_BUFFER_POOL_SIZE * sizeof(BufferDesc));
    wr.blockSize = blockSize;
    return wr;
}

static void WR_addBufDesc(WriteRegister* wr, const BufferDesc* bd)
{
    if (wr->buffers[wr->capacity-1].buf != NULL) {
        /* buffer capacity is full : extend it */
        size_t const oldCapacity = wr->capacity;
        size_t const addedCapacity = MIN(oldCapacity, 256);
        size_t const newCapacity = oldCapacity + addedCapacity;
        size_t const newSize = newCapacity * sizeof(BufferDesc);
        void* const newBuf = realloc(wr->buffers, newSize);
        if (newBuf == NULL) {
            END_PROCESS(39, "cannot extend register of buffers")
        }
        wr->buffers = (BufferDesc*)newBuf;
        memset(wr->buffers + oldCapacity, 0, addedCapacity * sizeof(BufferDesc));
        wr->buffers[oldCapacity] = bd[0];
        wr->capacity = newCapacity;
    } else {
        /* at least one position (the last one) is free, i.e. buffer==NULL */
        size_t n;
        for (n=0; n<wr->capacity; n++) {
            if (wr->buffers[n].buf == NULL) {
                wr->buffers[n] = bd[0];
                break;
            }
        }
        assert(n != wr->capacity);
    }
}

static int WR_isPresent(WriteRegister* wr, unsigned long long id)
{
    size_t n;
    for (n=0; n<wr->capacity; n++) {
        if (wr->buffers[n].buf == NULL) {
            /* no more buffers stored */
            return 0;
        }
        if (wr->buffers[n].rank == id)
            return 1;
    }
    return 0;
}

/* Note: requires @id to exist! */
static BufferDesc WR_getBufID(WriteRegister* wr, unsigned long long id)
{
    size_t n;
    for (n=0; n<wr->capacity; n++) {
        if (wr->buffers[n].buf == NULL) {
            /* no more buffers stored */
            break;
        }
        if (wr->buffers[n].rank == id)
            return wr->buffers[n];
    }
    END_PROCESS(41, "buffer ID not found");
}

static void WR_removeBuffID(WriteRegister* wr, unsigned long long id)
{
    size_t n;
    for (n=0; n<wr->capacity; n++) {
        if (wr->buffers[n].buf == NULL) {
            /* no more buffers stored */
            return;
        }
        if (wr->buffers[n].rank == id) {
            free(wr->buffers[n].buf);
            break;
        }
    }
    /* overwrite buffer descriptor, scale others down*/
    n++;
    for (; n < wr->capacity; n++) {
        wr->buffers[n-1] = wr->buffers[n];
        if (wr->buffers[n].buf == NULL)
            return;
    }
    {   BufferDesc const nullBd = { NULL, 0, 0 };
        wr->buffers[wr->capacity-1] = nullBd;
    }
}

typedef struct {
    WriteRegister* wr;
    void* cBuf;
    size_t cSize;
    unsigned long long blockNb;
    FILE* out;
} WriteJobDesc;

static void LZ4IO_writeBuffer(BufferDesc bufDesc, FILE* out)
{
    size_t const size = bufDesc.size;
    if (fwrite(bufDesc.buf, 1, size, out) != size) {
        END_PROCESS(38, "Write error : cannot write compressed block");
    }
}

static void LZ4IO_checkWriteOrder(void* arg)
{
    WriteJobDesc* const wjd = (WriteJobDesc*)arg;
    size_t const cSize = wjd->cSize;
    WriteRegister* const wr = wjd->wr;

    if (wjd->blockNb != wr->expectedRank) {
        /* incorrect order : let's store this buffer for later write */
        BufferDesc bd;
        bd.buf = wjd->cBuf;
        bd.size = wjd->cSize;
        bd.rank = wjd->blockNb;
        WR_addBufDesc(wr, &bd);
        free(wjd);  /* because wjd is pod */
        return;
    }

    /* expected block ID : let's write this block */
    {   BufferDesc bd;
        bd.buf = wjd->cBuf;
        bd.size = wjd->cSize;
        bd.rank = wjd->blockNb;
        LZ4IO_writeBuffer(bd, wjd->out);
    }
    wr->expectedRank++;
    wr->totalCSize += cSize;
    free(wjd->cBuf);
    /* and check for more blocks, previously saved */
    while (WR_isPresent(wr, wr->expectedRank)) {
        BufferDesc const bd = WR_getBufID(wr, wr->expectedRank);
        LZ4IO_writeBuffer(bd, wjd->out);
        wr->totalCSize += bd.size;
        WR_removeBuffID(wr, wr->expectedRank);
        wr->expectedRank++;
    }
    free(wjd);  /* because wjd is pod */
    {   unsigned long long const processedSize = (unsigned long long)(wr->expectedRank-1) * wr->blockSize;
        DISPLAYUPDATE(2, "\rRead : %u MiB   ==> %.2f%%   ",
                (unsigned)(processedSize >> 20),
                (double)wr->totalCSize / (double)processedSize * 100.);
    }
}

typedef size_t (*compress_f)(
    const void* parameters,
    void* dst,
    size_t dstCapacity,
    const void* src,
    size_t srcSize,
    size_t prefixSize);

typedef struct {
    TPool* wpool;
    void* buffer;
    size_t prefixSize;
    size_t inSize;
    unsigned long long blockNb;
    compress_f compress;
    const void* compressParameters;
    FILE* fout;
    WriteRegister* wr;
    size_t maxCBlockSize;
    int lastBlock;
} CompressJobDesc;

static void LZ4IO_compressChunk(void* arg)
{
    CompressJobDesc* const cjd = (CompressJobDesc*)arg;
    size_t const outCapacity = cjd->maxCBlockSize;
    void* const out_buff = malloc(outCapacity);
    if (!out_buff)
        END_PROCESS(33, "Allocation error : can't allocate output buffer to compress new chunk");
    {   char* const inBuff = (char*)cjd->buffer + cjd->prefixSize;
        size_t const cSize = cjd->compress(cjd->compressParameters, out_buff, outCapacity, inBuff, cjd->inSize, cjd->prefixSize);

        /* check for write */
        {   WriteJobDesc* const wjd = (WriteJobDesc*)malloc(sizeof(*wjd));
            if (wjd == NULL) {
                END_PROCESS(35, "Allocation error : can't describe new write job");
            }
            wjd->cBuf = out_buff;
            wjd->cSize = (size_t)cSize;
            wjd->blockNb = cjd->blockNb;
            wjd->out = cjd->fout;
            wjd->wr = cjd->wr;
            TPool_submitJob(cjd->wpool, LZ4IO_checkWriteOrder, wjd);
    }   }
}

static void LZ4IO_compressAndFreeChunk(void* arg)
{
    CompressJobDesc* const cjd = (CompressJobDesc*)arg;
    LZ4IO_compressChunk(arg);
    /* clean up */
    free(cjd->buffer);
    free(cjd); /* because cjd is pod */
}

/* one ReadTracker per file to compress */
typedef struct {
    TPool* tPool;
    TPool* wpool;
    FILE* fin;
    size_t chunkSize;
    unsigned long long totalReadSize;
    unsigned long long blockNb;
    XXH32_state_t* xxh32;
    compress_f compress;
    const void* compressParameters;
    void* prefix; /* if it exists, assumed to be filled with 64 KB */
    FILE* fout;
    WriteRegister* wr;
    size_t maxCBlockSize;
} ReadTracker;

static void LZ4IO_readAndProcess(void* arg)
{
    ReadTracker* const rjd = (ReadTracker*)arg;
    size_t const chunkSize = rjd->chunkSize;
    size_t const prefixSize = (rjd->prefix != NULL) * 64 KB;
    size_t const bufferSize = chunkSize + prefixSize;
    void* const buffer = malloc(bufferSize);
    if (!buffer)
        END_PROCESS(31, "Allocation error : can't allocate buffer to read new chunk");
    if (prefixSize) {
        assert(prefixSize == 64 KB);
        memcpy(buffer, rjd->prefix, prefixSize);
    }
    {   char* const in_buff = (char*)buffer + prefixSize;
        size_t const inSize = fread(in_buff, (size_t)1, chunkSize, rjd->fin);
        if (inSize > chunkSize) {
            END_PROCESS(32, "Read error (read %u > %u [chunk size])", (unsigned)inSize, (unsigned)chunkSize);
        }
        rjd->totalReadSize += inSize;
        /* special case: nothing left: stop read operation */
        if (inSize == 0) {
            free(buffer);
            return;
        }
        /* process read input */
        {   CompressJobDesc* const cjd = (CompressJobDesc*)malloc(sizeof(*cjd));
            if (cjd==NULL) {
                END_PROCESS(33, "Allocation error : can't describe new compression job");
            }
            if (rjd->xxh32) {
                XXH32_update(rjd->xxh32, in_buff, inSize);
            }
            if (rjd->prefix) {
                /* dependent blocks mode */
                memcpy(rjd->prefix, in_buff + inSize - 64 KB, 64 KB);
            }
            cjd->wpool = rjd->wpool;
            cjd->buffer = buffer; /* transfer ownership */
            cjd->prefixSize = prefixSize;
            cjd->inSize = inSize;
            cjd->blockNb = rjd->blockNb;
            cjd->compress = rjd->compress;
            cjd->compressParameters = rjd->compressParameters;
            cjd->fout = rjd->fout;
            cjd->wr = rjd->wr;
            cjd->maxCBlockSize = rjd->maxCBlockSize;
            cjd->lastBlock = inSize < chunkSize;
            TPool_submitJob(rjd->tPool, LZ4IO_compressAndFreeChunk, cjd);
            if (inSize == chunkSize) {
                /* likely more => read another chunk */
                rjd->blockNb++;
                TPool_submitJob(rjd->tPool, LZ4IO_readAndProcess, rjd);
    }   }   }
}


/***************************************
*   Legacy Compression
***************************************/

/* Size in bytes of a legacy block header in little-endian format */
#define LZ4IO_LEGACY_BLOCK_HEADER_SIZE 4
#define LZ4IO_LEGACY_BLOCK_SIZE_MAX  (8 MB)

/* unoptimized version; solves endianness & alignment issues */
static void LZ4IO_writeLE32 (void* p, unsigned value32)
{
    unsigned char* const dstPtr = (unsigned char*)p;
    dstPtr[0] = (unsigned char)value32;
    dstPtr[1] = (unsigned char)(value32 >> 8);
    dstPtr[2] = (unsigned char)(value32 >> 16);
    dstPtr[3] = (unsigned char)(value32 >> 24);
}


typedef struct {
    int cLevel;
} CompressLegacyState;

static size_t LZ4IO_compressBlockLegacy_fast(
    const void* params,
    void* dst,
    size_t dstCapacity,
    const void* src,
    size_t srcSize,
    size_t prefixSize
)
{
    const CompressLegacyState* const clevel = (const CompressLegacyState*)params;
    int const acceleration = (clevel->cLevel < 0) ? -clevel->cLevel : 0;
    int const cSize = LZ4_compress_fast((const char*)src, (char*)dst + LZ4IO_LEGACY_BLOCK_HEADER_SIZE, (int)srcSize, (int)dstCapacity, acceleration);
    if (cSize < 0)
        END_PROCESS(51, "fast compression failed");
    LZ4IO_writeLE32(dst, (unsigned)cSize);
    assert(prefixSize == 0); (void)prefixSize;
    return (size_t) cSize + LZ4IO_LEGACY_BLOCK_HEADER_SIZE;
}

static size_t LZ4IO_compressBlockLegacy_HC(
    const void* params,
    void* dst,
    size_t dstCapacity,
    const void* src,
    size_t srcSize,
    size_t prefixSize
)
{
    const CompressLegacyState* const cs = (const CompressLegacyState*)params;
    int const clevel = cs->cLevel;
    int const cSize = LZ4_compress_HC((const char*)src, (char*)dst + LZ4IO_LEGACY_BLOCK_HEADER_SIZE, (int)srcSize, (int)dstCapacity, clevel);
    if (cSize < 0)
        END_PROCESS(52, "HC compression failed");
    LZ4IO_writeLE32(dst, (unsigned)cSize);
    assert(prefixSize == 0); (void)prefixSize;
    return (size_t) cSize + LZ4IO_LEGACY_BLOCK_HEADER_SIZE;
}

/* LZ4IO_compressLegacy_internal :
 * Implementation of LZ4IO_compressFilename_Legacy.
 * @return: 0 if success, !0 if error
 */
static int LZ4IO_compressLegacy_internal(unsigned long long* readSize,
                                  const char* input_filename,
                                  const char* output_filename,
                                  int compressionlevel,
                                  const LZ4IO_prefs_t* prefs)
{
    int clResult = 0;
    compress_f const compressionFunction = (compressionlevel < 3) ? LZ4IO_compressBlockLegacy_fast : LZ4IO_compressBlockLegacy_HC;
    FILE* const finput = LZ4IO_openSrcFile(input_filename);
    FILE* foutput = NULL;
    TPool* const tPool = TPool_create(prefs->nbWorkers, 4);
    TPool* const wPool = TPool_create(1, 4);
    WriteRegister wr = WR_init(LEGACY_BLOCKSIZE);

    /* Init & checks */
    *readSize = 0;
    if (finput == NULL) {
        /* read file error : recoverable */
        clResult = 1;
        goto _cfl_clean;
    }
    foutput = LZ4IO_openDstFile(output_filename, prefs);
    if (foutput == NULL) {
        /* write file error : recoverable */
        clResult = 1;
        goto _cfl_clean;
    }
    if (tPool == NULL || wPool == NULL)
        END_PROCESS(21, "threadpool creation error ");
    if (wr.buffers == NULL)
        END_PROCESS(22, "can't allocate write register");


    /* Write Archive Header */
    {   char outHeader[MAGICNUMBER_SIZE];
        LZ4IO_writeLE32(outHeader, LEGACY_MAGICNUMBER);
        if (fwrite(outHeader, 1, MAGICNUMBER_SIZE, foutput) != MAGICNUMBER_SIZE)
            END_PROCESS(23, "Write error : cannot write header");
    }
    wr.totalCSize = MAGICNUMBER_SIZE;

    {   CompressLegacyState cls;
        ReadTracker rjd;
        cls.cLevel = compressionlevel;
        rjd.tPool = tPool;
        rjd.wpool = wPool;
        rjd.fin = finput;
        rjd.chunkSize = LEGACY_BLOCKSIZE;
        rjd.totalReadSize = 0;
        rjd.blockNb = 0;
        rjd.xxh32 = NULL;
        rjd.compress = compressionFunction;
        rjd.compressParameters = &cls;
        rjd.prefix = NULL;
        rjd.fout = foutput;
        rjd.wr = &wr;
        rjd.maxCBlockSize = (size_t)LZ4_compressBound(LEGACY_BLOCKSIZE) + LZ4IO_LEGACY_BLOCK_HEADER_SIZE;
        /* Ignite the job chain */
        TPool_submitJob(tPool, LZ4IO_readAndProcess, &rjd);
        /* Wait for all completion */
        TPool_jobsCompleted(tPool);
        TPool_jobsCompleted(wPool);

        /* Status */
        DISPLAYLEVEL(2, "\r%79s\r", "");    /* blank line */
        DISPLAYLEVEL(2,"Compressed %llu bytes into %llu bytes ==> %.2f%% \n",
                    rjd.totalReadSize, wr.totalCSize,
                    (double)wr.totalCSize / (double)(rjd.totalReadSize + !rjd.totalReadSize) * 100.);
        *readSize = rjd.totalReadSize;
    }

    /* Close & Free */
_cfl_clean:
    WR_destroy(&wr);
    TPool_free(wPool);
    TPool_free(tPool);
    if (finput) fclose(finput);
    if (foutput && !LZ4IO_isStdout(output_filename)) fclose(foutput);  /* do not close stdout */

    return clResult;
}

/* LZ4IO_compressFilename_Legacy :
 * This function is intentionally "hidden" (not published in .h)
 * It generates compressed streams using the old 'legacy' format
 * @return: 0 if success, !0 if error
 */
int LZ4IO_compressFilename_Legacy(const char* input_filename,
                                  const char* output_filename,
                                  int compressionlevel,
                                  const LZ4IO_prefs_t* prefs)
{
    TIME_t const timeStart = TIME_getTime();
    double const cpuStart = cpuTime_sec();
    unsigned long long processed = 0;
    int r = LZ4IO_compressLegacy_internal(&processed, input_filename, output_filename, compressionlevel, prefs);
    LZ4IO_finalTimeDisplay(timeStart, cpuStart, processed);
    return r;
}

#define FNSPACE 30
/* LZ4IO_compressMultipleFilenames_Legacy :
 * This function is intentionally "hidden" (not published in .h)
 * It generates multiple compressed streams using the old 'legacy' format */
int LZ4IO_compressMultipleFilenames_Legacy(
                            const char** inFileNamesTable, int ifntSize,
                            const char* suffix,
                            int compressionLevel, const LZ4IO_prefs_t* prefs)
{
    TIME_t const timeStart = TIME_getTime();
    double const cpuStart = cpuTime_sec();
    unsigned long long totalProcessed = 0;
    int i;
    int missed_files = 0;
    char* dstFileName = (char*)malloc(FNSPACE);
    size_t ofnSize = FNSPACE;
    const size_t suffixSize = strlen(suffix);

    if (dstFileName == NULL) return ifntSize;   /* not enough memory */

    /* loop on each file */
    for (i=0; i<ifntSize; i++) {
        unsigned long long processed = 0;
        size_t const ifnSize = strlen(inFileNamesTable[i]);
        if (LZ4IO_isStdout(suffix)) {
            missed_files += LZ4IO_compressLegacy_internal(&processed,
                                    inFileNamesTable[i], stdoutmark,
                                    compressionLevel, prefs);
            totalProcessed += processed;
            continue;
        }

        if (ofnSize <= ifnSize+suffixSize+1) {
            free(dstFileName);
            ofnSize = ifnSize + 20;
            dstFileName = (char*)malloc(ofnSize);
            if (dstFileName==NULL) {
                return ifntSize;
        }   }
        strcpy(dstFileName, inFileNamesTable[i]);
        strcat(dstFileName, suffix);

        missed_files += LZ4IO_compressLegacy_internal(&processed,
                                inFileNamesTable[i], dstFileName,
                                compressionLevel, prefs);
        totalProcessed += processed;
    }

    /* Close & Free */
    LZ4IO_finalTimeDisplay(timeStart, cpuStart, totalProcessed);
    free(dstFileName);

    return missed_files;
}

/*********************************************
*  Compression using Frame format
*********************************************/
typedef struct {
    void*  srcBuffer;
    size_t srcBufferSize;
    void*  dstBuffer;
    size_t dstBufferSize;
    LZ4F_compressionContext_t ctx;
    LZ4F_preferences_t preparedPrefs;
    LZ4F_CDict* cdict;
    TPool* tPool;
    TPool* wPool; /* writer thread */
} cRess_t;

static void LZ4IO_freeCResources(cRess_t ress)
{
    TPool_free(ress.tPool);
    TPool_free(ress.wPool);

    free(ress.srcBuffer);
    free(ress.dstBuffer);

    LZ4F_freeCDict(ress.cdict);
    ress.cdict = NULL;

    { LZ4F_errorCode_t const errorCode = LZ4F_freeCompressionContext(ress.ctx);
      if (LZ4F_isError(errorCode)) END_PROCESS(35, "Error : can't free LZ4F context resource : %s", LZ4F_getErrorName(errorCode)); }
}

static void* LZ4IO_createDict(size_t* dictSize, const char* dictFilename)
{
    size_t readSize;
    size_t dictEnd = 0;
    size_t dictLen = 0;
    size_t dictStart;
    size_t circularBufSize = LZ4_MAX_DICT_SIZE;
    char* circularBuf = (char*)malloc(circularBufSize);
    char* dictBuf;
    FILE* dictFile;

    if (!dictFilename)
        END_PROCESS(26, "Dictionary error : no filename provided");
    if (!circularBuf)
        END_PROCESS(25, "Allocation error : not enough memory for circular buffer");

    dictFile = LZ4IO_openSrcFile(dictFilename);
    if (!dictFile)
        END_PROCESS(27, "Dictionary error : could not open dictionary file");

    /* opportunistically seek to the part of the file we care about.
     * If this fails it's not a problem since we'll just read everything anyways. */
    if (!LZ4IO_isStdin(dictFilename)) {
        (void)UTIL_fseek(dictFile, -LZ4_MAX_DICT_SIZE, SEEK_END);
    }

    do {
        readSize = fread(circularBuf + dictEnd, 1, circularBufSize - dictEnd, dictFile);
        dictEnd = (dictEnd + readSize) % circularBufSize;
        dictLen += readSize;
    } while (readSize>0);

    if (dictLen > LZ4_MAX_DICT_SIZE) {
        dictLen = LZ4_MAX_DICT_SIZE;
    }

    *dictSize = dictLen;

    dictStart = (circularBufSize + dictEnd - dictLen) % circularBufSize;

    if (dictStart == 0) {
        /* We're in the simple case where the dict starts at the beginning of our circular buffer. */
        dictBuf = circularBuf;
        circularBuf = NULL;
    } else {
        /* Otherwise, we will alloc a new buffer and copy our dict into that. */
        dictBuf = (char *)malloc(dictLen ? dictLen : 1);
        if (!dictBuf) END_PROCESS(28, "Allocation error : not enough memory");

        memcpy(dictBuf, circularBuf + dictStart, circularBufSize - dictStart);
        memcpy(dictBuf + circularBufSize - dictStart, circularBuf, dictLen - (circularBufSize - dictStart));
    }

    fclose(dictFile);
    free(circularBuf);

    return dictBuf;
}

static LZ4F_CDict* LZ4IO_createCDict(const LZ4IO_prefs_t* io_prefs)
{
    size_t dictionarySize;
    void* dictionaryBuffer;
    LZ4F_CDict* cdict;
    if (!io_prefs->useDictionary) return NULL;
    dictionaryBuffer = LZ4IO_createDict(&dictionarySize, io_prefs->dictionaryFilename);
    if (!dictionaryBuffer) END_PROCESS(29, "Dictionary error : could not create dictionary");
    cdict = LZ4F_createCDict(dictionaryBuffer, dictionarySize);
    free(dictionaryBuffer);
    return cdict;
}

static cRess_t LZ4IO_createCResources(const LZ4IO_prefs_t* io_prefs)
{
    const size_t chunkSize = 4 MB;
    cRess_t ress;
    memset(&ress, 0, sizeof(ress));

    /* set compression advanced parameters */
    ress.preparedPrefs.autoFlush = 1;
    ress.preparedPrefs.frameInfo.blockMode = (LZ4F_blockMode_t)io_prefs->blockIndependence;
    ress.preparedPrefs.frameInfo.blockSizeID = (LZ4F_blockSizeID_t)io_prefs->blockSizeId;
    ress.preparedPrefs.frameInfo.blockChecksumFlag = (LZ4F_blockChecksum_t)io_prefs->blockChecksum;
    ress.preparedPrefs.frameInfo.contentChecksumFlag = (LZ4F_contentChecksum_t)io_prefs->streamChecksum;
    ress.preparedPrefs.favorDecSpeed = io_prefs->favorDecSpeed;

    /* Allocate compression state */
    {   LZ4F_errorCode_t const errorCode = LZ4F_createCompressionContext(&(ress.ctx), LZ4F_VERSION);
        if (LZ4F_isError(errorCode))
            END_PROCESS(30, "Allocation error : can't create LZ4F context : %s", LZ4F_getErrorName(errorCode));
    }
    assert(ress.ctx != NULL);

    /* Allocate Buffers */
    ress.srcBuffer = malloc(chunkSize);
    ress.srcBufferSize = chunkSize;
    ress.dstBufferSize = LZ4F_compressFrameBound(chunkSize, &ress.preparedPrefs);
    ress.dstBuffer = malloc(ress.dstBufferSize);
    if (!ress.srcBuffer || !ress.dstBuffer)
        END_PROCESS(31, "Allocation error : can't allocate buffers");

    ress.cdict = LZ4IO_createCDict(io_prefs);

    /* will be created it needed */
    ress.tPool = NULL;
    ress.wPool = NULL;

    return ress;
}

typedef struct {
    const LZ4F_preferences_t* prefs;
    const LZ4F_CDict* cdict;
} LZ4IO_CfcParameters;

static size_t LZ4IO_compressFrameChunk(const void* params,
                                    void* dst, size_t dstCapacity,
                                    const void* src, size_t srcSize,
                                    size_t prefixSize)
{
    const LZ4IO_CfcParameters* const cfcp = (const LZ4IO_CfcParameters*)params;
    LZ4F_cctx* cctx = NULL;
    {   LZ4F_errorCode_t const ccr = LZ4F_createCompressionContext(&cctx, LZ4F_VERSION);
        if (cctx==NULL || LZ4F_isError(ccr))
            END_PROCESS(51, "unable to create a LZ4F compression context");
    }
    /* init state, and writes frame header, will be overwritten at next stage. */
    if (prefixSize) {
        size_t const whr = LZ4F_compressBegin_usingDict(cctx, dst, dstCapacity, (const char*)src - prefixSize, prefixSize, cfcp->prefs);
        if (LZ4F_isError(whr))
            END_PROCESS(52, "error initializing LZ4F compression context with prefix");
        assert(prefixSize == 64 KB);
    } else {
        size_t const whr = LZ4F_compressBegin_usingCDict(cctx, dst, dstCapacity, cfcp->cdict, cfcp->prefs);
        if (LZ4F_isError(whr))
            END_PROCESS(53, "error initializing LZ4F compression context");
    }
    /* let's now compress, overwriting unused header */
    {   size_t const cSize = LZ4F_compressUpdate(cctx, dst, dstCapacity, src, srcSize, NULL);
        if (LZ4F_isError(cSize))
            END_PROCESS(55, "error compressing with LZ4F_compressUpdate");

        LZ4F_freeCompressionContext(cctx);
        return (size_t) cSize;
    }
}

/*
 * LZ4IO_compressFilename_extRess()
 * result : 0 : compression completed correctly
 *          1 : missing or pb opening srcFileName
 */
int
LZ4IO_compressFilename_extRess_MT(unsigned long long* inStreamSize,
                               cRess_t* ress,
                               const char* srcFileName, const char* dstFileName,
                               int compressionLevel,
                               const LZ4IO_prefs_t* const io_prefs)
{
    unsigned long long filesize = 0;
    unsigned long long compressedfilesize = 0;
    FILE* dstFile;
    void* const srcBuffer = ress->srcBuffer;
    void* const dstBuffer = ress->dstBuffer;
    const size_t dstBufferSize = ress->dstBufferSize;
    const size_t chunkSize = 4 MB;  /* each job should be "sufficiently large" */
    size_t readSize;
    LZ4F_compressionContext_t ctx = ress->ctx;   /* just a pointer */
    LZ4F_preferences_t prefs;

    /* Init */
    FILE* const srcFile = LZ4IO_openSrcFile(srcFileName);
    if (srcFile == NULL) return 1;
    dstFile = LZ4IO_openDstFile(dstFileName, io_prefs);
    if (dstFile == NULL) { fclose(srcFile); return 1; }

    /* Adjust compression parameters */
    prefs = ress->preparedPrefs;
    prefs.compressionLevel = compressionLevel;
    if (io_prefs->contentSizeFlag) {
      U64 const fileSize = UTIL_getOpenFileSize(srcFile);
      prefs.frameInfo.contentSize = fileSize;   /* == 0 if input == stdin */
      if (fileSize==0)
          DISPLAYLEVEL(3, "Warning : cannot determine input content size \n");
    }

    /* read first chunk */
    assert(chunkSize <= ress->srcBufferSize);
    readSize  = fread(srcBuffer, (size_t)1, chunkSize, srcFile);
    if (ferror(srcFile))
        END_PROCESS(40, "Error reading first chunk (%u bytes) of '%s' ", (unsigned)chunkSize, srcFileName);
    filesize += readSize;

    /* single-block file */
    if (readSize < chunkSize) {
        /* Compress in single pass */
        size_t const cSize = LZ4F_compressFrame_usingCDict(ctx, dstBuffer, dstBufferSize, srcBuffer, readSize, ress->cdict, &prefs);
        if (LZ4F_isError(cSize))
            END_PROCESS(41, "Compression failed : %s", LZ4F_getErrorName(cSize));
        compressedfilesize = cSize;
        DISPLAYUPDATE(2, "\rRead : %u MiB   ==> %.2f%%   ",
                      (unsigned)(filesize>>20), (double)compressedfilesize/(double)(filesize+!filesize)*100);   /* avoid division by zero */

        /* Write Block */
        if (fwrite(dstBuffer, 1, cSize, dstFile) != cSize) {
            END_PROCESS(42, "Write error : failed writing single-block compressed frame");
    }   }

    else

    /* multiple-blocks file */
    {   WriteRegister wr = WR_init(chunkSize);
        void* prefixBuffer = NULL;

        int checksum = (int)prefs.frameInfo.contentChecksumFlag;
        XXH32_state_t* xxh32 = NULL;

        LZ4IO_CfcParameters cfcp;
        ReadTracker rjd;

        if (ress->tPool == NULL) {
            ress->tPool = TPool_create(io_prefs->nbWorkers, 4);
            assert(ress->wPool == NULL);
            ress->wPool = TPool_create(1, 4);
            if (ress->tPool == NULL || ress->wPool == NULL)
                END_PROCESS(43, "can't create threadpools");
        }
        cfcp.prefs = &prefs;
        cfcp.cdict = ress->cdict;
        rjd.tPool = ress->tPool;
        rjd.wpool = ress->wPool;
        rjd.fin = srcFile;
        rjd.chunkSize = chunkSize;
        rjd.totalReadSize = 0;
        rjd.blockNb = 0;
        rjd.xxh32 = xxh32;
        rjd.compress = LZ4IO_compressFrameChunk;
        rjd.compressParameters = &cfcp;
        rjd.prefix = NULL;
        rjd.fout = dstFile;
        rjd.wr = &wr;
        rjd.maxCBlockSize = LZ4F_compressFrameBound(chunkSize, &prefs);

        /* process frame checksum externally */
        if (checksum) {
            xxh32 = XXH32_createState();
            if (xxh32==NULL)
                END_PROCESS(42, "could not init checksum");
            XXH32_reset(xxh32, 0);
            XXH32_update(xxh32, srcBuffer, readSize);
            rjd.xxh32 = xxh32;
        }

        /* block dependency */
        if (prefs.frameInfo.blockMode == LZ4F_blockLinked) {
            prefixBuffer = malloc(64 KB);
            if (prefixBuffer==NULL)
                END_PROCESS(43, "cannot allocate small dictionary buffer");
            rjd.prefix = prefixBuffer;
        }

        /* Write Frame Header */
        /* note: simplification: do not employ dictionary when input size >= 4 MB,
         * the benefit is very limited anyway, and is not worth the dependency cost */
        {   size_t const headerSize = LZ4F_compressBegin(ctx, dstBuffer, dstBufferSize, &prefs);
            if (LZ4F_isError(headerSize))
                END_PROCESS(44, "File header generation failed : %s", LZ4F_getErrorName(headerSize));
            if (fwrite(dstBuffer, 1, headerSize, dstFile) != headerSize)
                END_PROCESS(45, "Write error : cannot write header");
            compressedfilesize = headerSize;
        }
        /* avoid duplicating effort to process content checksum (done externally) */
        prefs.frameInfo.contentChecksumFlag = LZ4F_noContentChecksum;

        /* process first block */
        {   CompressJobDesc cjd;
            cjd.wpool = ress->wPool;
            cjd.buffer = srcBuffer;
            cjd.prefixSize = 0;
            cjd.inSize = readSize;
            cjd.blockNb = 0;
            cjd.compress = LZ4IO_compressFrameChunk;
            cjd.compressParameters = &cfcp;
            cjd.fout = dstFile;
            cjd.wr = &wr;
            cjd.maxCBlockSize = rjd.maxCBlockSize;
            cjd.lastBlock = 0;
            TPool_submitJob(ress->tPool, LZ4IO_compressChunk, &cjd);
            rjd.totalReadSize = readSize;
            rjd.blockNb = 1;
            if (prefixBuffer) {
                assert(readSize >= 64 KB);
                memcpy(prefixBuffer, (char*)srcBuffer + readSize - 64 KB, 64 KB);
            }

            /* Start the job chain */
            TPool_submitJob(ress->tPool, LZ4IO_readAndProcess, &rjd);

            /* Wait for all completion */
            TPool_jobsCompleted(ress->tPool);
            TPool_jobsCompleted(ress->wPool);
            compressedfilesize += wr.totalCSize;
        }

        /* End of Frame mark */
        {   size_t endSize = 4;
            assert(dstBufferSize >= 8);
            memset(dstBuffer, 0, 4);
            if (checksum) {
                /* handle frame checksum externally
                 * note: LZ4F_compressEnd already wrote a (bogus) checksum */
                U32 const crc = XXH32_digest(xxh32);
                LZ4IO_writeLE32( (char*)dstBuffer + 4, crc);
                endSize = 8;
            }
            if (fwrite(dstBuffer, 1, endSize, dstFile) != endSize)
                END_PROCESS(49, "Write error : cannot write end of frame");
            compressedfilesize += endSize;
            filesize = rjd.totalReadSize;
        }

        /* clean up*/
        free(prefixBuffer);
        XXH32_freeState(xxh32);
        WR_destroy(&wr);
    }

    /* Release file handlers */
    fclose (srcFile);
    if (!LZ4IO_isStdout(dstFileName)) fclose(dstFile);  /* do not close stdout */

    /* Copy owner, file permissions and modification time */
    {   stat_t statbuf;
        if (!LZ4IO_isStdin(srcFileName)
         && !LZ4IO_isStdout(dstFileName)
         && !LZ4IO_isDevNull(dstFileName)
         && UTIL_getFileStat(srcFileName, &statbuf)) {
            UTIL_setFileStat(dstFileName, &statbuf);
    }   }

    if (io_prefs->removeSrcFile) {  /* remove source file : --rm */
        if (remove(srcFileName))
            END_PROCESS(50, "Remove error : %s: %s", srcFileName, strerror(errno));
    }

    /* Final Status */
    DISPLAYLEVEL(2, "\r%79s\r", "");
    DISPLAYLEVEL(2, "Compressed %llu bytes into %llu bytes ==> %.2f%%\n",
                    filesize, compressedfilesize,
                    (double)compressedfilesize / (double)(filesize + !filesize /* avoid division by zero */ ) * 100.);
    *inStreamSize = filesize;

    return 0;
}

/*
 * LZ4IO_compressFilename_extRess()
 * result : 0 : compression completed correctly
 *          1 : missing or pb opening srcFileName
 */
int
LZ4IO_compressFilename_extRess_ST(unsigned long long* inStreamSize,
                               const cRess_t* ress,
                               const char* srcFileName, const char* dstFileName,
                               int compressionLevel,
                               const LZ4IO_prefs_t* const io_prefs)
{
    unsigned long long filesize = 0;
    unsigned long long compressedfilesize = 0;
    FILE* dstFile;
    void* const srcBuffer = ress->srcBuffer;
    void* const dstBuffer = ress->dstBuffer;
    const size_t dstBufferSize = ress->dstBufferSize;
    const size_t blockSize = io_prefs->blockSize;
    size_t readSize;
    LZ4F_compressionContext_t ctx = ress->ctx;   /* just a pointer */
    LZ4F_preferences_t prefs;

    /* Init */
    FILE* const srcFile = LZ4IO_openSrcFile(srcFileName);
    if (srcFile == NULL) return 1;
    dstFile = LZ4IO_openDstFile(dstFileName, io_prefs);
    if (dstFile == NULL) { fclose(srcFile); return 1; }
    memset(&prefs, 0, sizeof(prefs));

    /* Adjust compression parameters */
    prefs = ress->preparedPrefs;
    prefs.compressionLevel = compressionLevel;
    if (io_prefs->contentSizeFlag) {
      U64 const fileSize = UTIL_getOpenFileSize(srcFile);
      prefs.frameInfo.contentSize = fileSize;   /* == 0 if input == stdin */
      if (fileSize==0)
          DISPLAYLEVEL(3, "Warning : cannot determine input content size \n");
    }

    /* read first block */
    readSize  = fread(srcBuffer, (size_t)1, blockSize, srcFile);
    if (ferror(srcFile)) END_PROCESS(40, "Error reading %s ", srcFileName);
    filesize += readSize;

    /* single-block file */
    if (readSize < blockSize) {
        /* Compress in single pass */
        size_t const cSize = LZ4F_compressFrame_usingCDict(ctx, dstBuffer, dstBufferSize, srcBuffer, readSize, ress->cdict, &prefs);
        if (LZ4F_isError(cSize))
            END_PROCESS(41, "Compression failed : %s", LZ4F_getErrorName(cSize));
        compressedfilesize = cSize;
        DISPLAYUPDATE(2, "\rRead : %u MiB   ==> %.2f%%   ",
                      (unsigned)(filesize>>20), (double)compressedfilesize/(double)(filesize+!filesize)*100);   /* avoid division by zero */

        /* Write Block */
        if (fwrite(dstBuffer, 1, cSize, dstFile) != cSize) {
            END_PROCESS(42, "Write error : failed writing single-block compressed frame");
    }   }

    else

    /* multiple-blocks file */
    {
        /* Write Frame Header */
        size_t const headerSize = LZ4F_compressBegin_usingCDict(ctx, dstBuffer, dstBufferSize, ress->cdict, &prefs);
        if (LZ4F_isError(headerSize))
            END_PROCESS(43, "File header generation failed : %s", LZ4F_getErrorName(headerSize));
        if (fwrite(dstBuffer, 1, headerSize, dstFile) != headerSize)
            END_PROCESS(44, "Write error : cannot write header");
        compressedfilesize += headerSize;

        /* Main Loop - one block at a time */
        while (readSize>0) {
            size_t const outSize = LZ4F_compressUpdate(ctx, dstBuffer, dstBufferSize, srcBuffer, readSize, NULL);
            if (LZ4F_isError(outSize))
                END_PROCESS(45, "Compression failed : %s", LZ4F_getErrorName(outSize));
            compressedfilesize += outSize;
            DISPLAYUPDATE(2, "\rRead : %u MiB   ==> %.2f%%   ",
                        (unsigned)(filesize>>20),
                        (double)compressedfilesize / (double)filesize * 100.);

            /* Write Block */
            if (fwrite(dstBuffer, 1, outSize, dstFile) != outSize)
                END_PROCESS(46, "Write error : cannot write compressed block");

            /* Read next block */
            readSize  = fread(srcBuffer, (size_t)1, (size_t)blockSize, srcFile);
            filesize += readSize;
        }
        if (ferror(srcFile)) END_PROCESS(47, "Error reading %s ", srcFileName);

        /* End of Frame mark */
        {   size_t const endSize = LZ4F_compressEnd(ctx, dstBuffer, dstBufferSize, NULL);
            if (LZ4F_isError(endSize))
                END_PROCESS(48, "End of frame error : %s", LZ4F_getErrorName(endSize));
            if (fwrite(dstBuffer, 1, endSize, dstFile) != endSize)
                END_PROCESS(49, "Write error : cannot write end of frame");
            compressedfilesize += endSize;
        }
    }

    /* Release file handlers */
    fclose (srcFile);
    if (!LZ4IO_isStdout(dstFileName)) fclose(dstFile);  /* do not close stdout */

    /* Copy owner, file permissions and modification time */
    {   stat_t statbuf;
        if (!LZ4IO_isStdin(srcFileName)
         && !LZ4IO_isStdout(dstFileName)
         && !LZ4IO_isDevNull(dstFileName)
         && UTIL_getFileStat(srcFileName, &statbuf)) {
            UTIL_setFileStat(dstFileName, &statbuf);
    }   }

    if (io_prefs->removeSrcFile) {  /* remove source file : --rm */
        if (remove(srcFileName))
            END_PROCESS(50, "Remove error : %s: %s", srcFileName, strerror(errno));
    }

    /* Final Status */
    DISPLAYLEVEL(2, "\r%79s\r", "");
    DISPLAYLEVEL(2, "Compressed %llu bytes into %llu bytes ==> %.2f%%\n",
                    filesize, compressedfilesize,
                    (double)compressedfilesize / (double)(filesize + !filesize /* avoid division by zero */ ) * 100.);
    *inStreamSize = filesize;

    return 0;
}

static int
LZ4IO_compressFilename_extRess(unsigned long long* inStreamSize,
                               cRess_t* ress,
                               const char* srcFileName, const char* dstFileName,
                               int compressionLevel,
                               const LZ4IO_prefs_t* const io_prefs)
{
    if (LZ4IO_MULTITHREAD)
        return LZ4IO_compressFilename_extRess_MT(inStreamSize, ress, srcFileName, dstFileName, compressionLevel, io_prefs);
    /* Only single-thread available */
    return LZ4IO_compressFilename_extRess_ST(inStreamSize, ress, srcFileName, dstFileName, compressionLevel, io_prefs);
}

int LZ4IO_compressFilename(const char* srcFileName, const char* dstFileName, int compressionLevel, const LZ4IO_prefs_t* prefs)
{
    TIME_t const timeStart = TIME_getTime();
    double const cpuStart = cpuTime_sec();
    cRess_t ress = LZ4IO_createCResources(prefs);
    unsigned long long processed;

    int const result = LZ4IO_compressFilename_extRess(&processed, &ress, srcFileName, dstFileName, compressionLevel, prefs);

    /* Free resources */
    LZ4IO_freeCResources(ress);

    /* Final Status */
    LZ4IO_finalTimeDisplay(timeStart, cpuStart, processed);

    return result;
}

int LZ4IO_compressMultipleFilenames(
                              const char** inFileNamesTable, int ifntSize,
                              const char* suffix,
                              int compressionLevel,
                              const LZ4IO_prefs_t* prefs)
{
    int i;
    int missed_files = 0;
    char* dstFileName = (char*)malloc(FNSPACE);
    size_t ofnSize = FNSPACE;
    const size_t suffixSize = strlen(suffix);
    cRess_t ress;
    unsigned long long totalProcessed = 0;
    TIME_t timeStart = TIME_getTime();
    double const cpuStart = cpuTime_sec();

    if (dstFileName == NULL) return ifntSize;   /* not enough memory */
    ress = LZ4IO_createCResources(prefs);

    /* loop on each file */
    for (i=0; i<ifntSize; i++) {
        unsigned long long processed;
        size_t const ifnSize = strlen(inFileNamesTable[i]);
        if (LZ4IO_isStdout(suffix)) {
            missed_files += LZ4IO_compressFilename_extRess(&processed, &ress,
                                    inFileNamesTable[i], stdoutmark,
                                    compressionLevel, prefs);
            totalProcessed += processed;
            continue;
        }
        /* suffix != stdout => compress into a file => generate its name */
        if (ofnSize <= ifnSize+suffixSize+1) {
            free(dstFileName);
            ofnSize = ifnSize + 20;
            dstFileName = (char*)malloc(ofnSize);
            if (dstFileName==NULL) {
                LZ4IO_freeCResources(ress);
                return ifntSize;
        }   }
        strcpy(dstFileName, inFileNamesTable[i]);
        strcat(dstFileName, suffix);

        missed_files += LZ4IO_compressFilename_extRess(&processed, &ress,
                                inFileNamesTable[i], dstFileName,
                                compressionLevel, prefs);
        totalProcessed += processed;
    }

    /* Close & Free */
    LZ4IO_freeCResources(ress);
    free(dstFileName);
    LZ4IO_finalTimeDisplay(timeStart, cpuStart, totalProcessed);

    return missed_files;
}


/* ********************************************************************* */
/* ********************** LZ4 file-stream Decompression **************** */
/* ********************************************************************* */

/* It's presumed that @p points to a memory space of size >= 4 */
static unsigned LZ4IO_readLE32 (const void* p)
{
    const unsigned char* const srcPtr = (const unsigned char*)p;
    unsigned value32 = srcPtr[0];
    value32 += (unsigned)srcPtr[1] <<  8;
    value32 += (unsigned)srcPtr[2] << 16;
    value32 += (unsigned)srcPtr[3] << 24;
    return value32;
}


static unsigned
LZ4IO_fwriteSparse(FILE* file,
                   const void* buffer, size_t bufferSize,
                   int sparseFileSupport,
                   unsigned storedSkips)
{
    const size_t sizeT = sizeof(size_t);
    const size_t maskT = sizeT -1 ;
    const size_t* const bufferT = (const size_t*)buffer;   /* Buffer is supposed malloc'ed, hence aligned on size_t */
    const size_t* ptrT = bufferT;
    size_t bufferSizeT = bufferSize / sizeT;
    const size_t* const bufferTEnd = bufferT + bufferSizeT;
    const size_t segmentSizeT = (32 KB) / sizeT;
    int const sparseMode = (sparseFileSupport - (file==stdout)) > 0;

    if (!sparseMode) {  /* normal write */
        size_t const sizeCheck = fwrite(buffer, 1, bufferSize, file);
        if (sizeCheck != bufferSize) END_PROCESS(70, "Write error : cannot write decoded block");
        return 0;
    }

    /* avoid int overflow */
    if (storedSkips > 1 GB) {
        int const seekResult = UTIL_fseek(file, 1 GB, SEEK_CUR);
        if (seekResult != 0) END_PROCESS(71, "1 GB skip error (sparse file support)");
        storedSkips -= 1 GB;
    }

    while (ptrT < bufferTEnd) {
        size_t seg0SizeT = segmentSizeT;
        size_t nb0T;

        /* count leading zeros */
        if (seg0SizeT > bufferSizeT) seg0SizeT = bufferSizeT;
        bufferSizeT -= seg0SizeT;
        for (nb0T=0; (nb0T < seg0SizeT) && (ptrT[nb0T] == 0); nb0T++) ;
        storedSkips += (unsigned)(nb0T * sizeT);

        if (nb0T != seg0SizeT) {   /* not all 0s */
            errno = 0;
            {   int const seekResult = UTIL_fseek(file, storedSkips, SEEK_CUR);
                if (seekResult) END_PROCESS(72, "Sparse skip error(%d): %s ; try --no-sparse", (int)errno, strerror(errno));
            }
            storedSkips = 0;
            seg0SizeT -= nb0T;
            ptrT += nb0T;
            {   size_t const sizeCheck = fwrite(ptrT, sizeT, seg0SizeT, file);
                if (sizeCheck != seg0SizeT) END_PROCESS(73, "Write error : cannot write decoded block");
        }   }
        ptrT += seg0SizeT;
    }

    if (bufferSize & maskT) {  /* size not multiple of sizeT : implies end of block */
        const char* const restStart = (const char*)bufferTEnd;
        const char* restPtr = restStart;
        size_t const restSize =  bufferSize & maskT;
        const char* const restEnd = restStart + restSize;
        for (; (restPtr < restEnd) && (*restPtr == 0); restPtr++) ;
        storedSkips += (unsigned) (restPtr - restStart);
        if (restPtr != restEnd) {
            int const seekResult = UTIL_fseek(file, storedSkips, SEEK_CUR);
            if (seekResult) END_PROCESS(74, "Sparse skip error ; try --no-sparse");
            storedSkips = 0;
            {   size_t const sizeCheck = fwrite(restPtr, 1, (size_t)(restEnd - restPtr), file);
                if (sizeCheck != (size_t)(restEnd - restPtr)) END_PROCESS(75, "Write error : cannot write decoded end of block");
        }   }
    }

    return storedSkips;
}

static void LZ4IO_fwriteSparseEnd(FILE* file, unsigned storedSkips)
{
    if (storedSkips>0) {   /* implies sparseFileSupport>0 */
        const char lastZeroByte[1] = { 0 };
        if (UTIL_fseek(file, storedSkips-1, SEEK_CUR) != 0)
            END_PROCESS(69, "Final skip error (sparse file)\n");
        if (fwrite(lastZeroByte, 1, 1, file) != 1)
            END_PROCESS(69, "Write error : cannot write last zero\n");
    }
}


static unsigned g_magicRead = 0;   /* out-parameter of LZ4IO_decodeLegacyStream() */

#if LZ4IO_MULTITHREAD

typedef struct {
    void* buffer;
    size_t size;
    FILE* f;
    int sparseEnable;
    unsigned* storedSkips;
    const unsigned long long* totalSize;
} ChunkToWrite;

static void LZ4IO_writeDecodedChunk(void* arg)
{
    ChunkToWrite* const ctw = (ChunkToWrite*)arg;
    assert(ctw != NULL);

    /* note: works because only 1 thread */
    *ctw->storedSkips = LZ4IO_fwriteSparse(ctw->f, ctw->buffer, ctw->size, ctw->sparseEnable, *ctw->storedSkips); /* success or die */
    DISPLAYUPDATE(2, "\rDecompressed : %u MiB  ", (unsigned)(ctw->totalSize[0] >>20));

    /* clean up */
    free(ctw);
}

typedef struct {
    void* inBuffer;
    size_t inSize;
    void* outBuffer;
    unsigned long long* totalSize;
    TPool* wPool;
    FILE* foutput;
    int sparseEnable;
    unsigned* storedSkips;
} LegacyBlockInput;

static void LZ4IO_decompressBlockLegacy(void* arg)
{
    int decodedSize;
    LegacyBlockInput* const lbi = (LegacyBlockInput*)arg;

    decodedSize = LZ4_decompress_safe((const char*)lbi->inBuffer, (char*)lbi->outBuffer, (int)lbi->inSize, LEGACY_BLOCKSIZE);
    if (decodedSize < 0) END_PROCESS(64, "Decoding Failed ! Corrupted input detected !");
    *lbi->totalSize += (unsigned long long)decodedSize; /* note: works because only 1 thread */

    /* push to write thread */
    {   ChunkToWrite* const ctw = (ChunkToWrite*)malloc(sizeof(*ctw));
        if (ctw==NULL) {
            END_PROCESS(33, "Allocation error : can't describe new write job");
        }
        ctw->buffer = lbi->outBuffer;
        ctw->size = (size_t)decodedSize;
        ctw->f = lbi->foutput;
        ctw->sparseEnable = lbi->sparseEnable;
        ctw->storedSkips = lbi->storedSkips;
        ctw->totalSize = lbi->totalSize;
        TPool_submitJob(lbi->wPool, LZ4IO_writeDecodedChunk, ctw);
    }

    /* clean up */
    free(lbi);
}

static unsigned long long
LZ4IO_decodeLegacyStream(FILE* finput, FILE* foutput, const LZ4IO_prefs_t* prefs)
{
    unsigned long long streamSize = 0;
    unsigned storedSkips = 0;

    TPool* const tPool = TPool_create(1, 1);
    TPool* const wPool = TPool_create(1, 1);
#define NB_BUFFSETS 4 /* 1 being read, 1 being processed, 1 being written, 1 being queued */
    void* inBuffs[NB_BUFFSETS];
    void* outBuffs[NB_BUFFSETS];
    int bSetNb;

    if (tPool == NULL || wPool == NULL)
        END_PROCESS(21, "threadpool creation error ");

    /* allocate buffers up front */
    for (bSetNb=0; bSetNb<NB_BUFFSETS; bSetNb++) {
        inBuffs[bSetNb] = malloc((size_t)LZ4_compressBound(LEGACY_BLOCKSIZE));
        outBuffs[bSetNb] = malloc(LEGACY_BLOCKSIZE);
        if (!inBuffs[bSetNb] || !outBuffs[bSetNb])
            END_PROCESS(31, "Allocation error : can't allocate buffer for legacy decoding");
    }

    /* Main Loop */
    for (bSetNb = 0;;bSetNb = (bSetNb+1) % NB_BUFFSETS) {
        char header[LZ4IO_LEGACY_BLOCK_HEADER_SIZE];
        unsigned int blockSize;

        /* Block Size */
        {   size_t const sizeCheck = fread(header, 1, LZ4IO_LEGACY_BLOCK_HEADER_SIZE, finput);
            if (sizeCheck == 0) break;                   /* Nothing to read : file read is completed */
            if (sizeCheck != LZ4IO_LEGACY_BLOCK_HEADER_SIZE)
                END_PROCESS(61, "Error: cannot read block size in Legacy format");
        }
        blockSize = LZ4IO_readLE32(header);       /* Convert to Little Endian */
        if (blockSize > LZ4_COMPRESSBOUND(LEGACY_BLOCKSIZE)) {
            /* Cannot read next block : maybe new stream ? */
            g_magicRead = blockSize;
            break;
        }

        /* Read Block */
        {   size_t const sizeCheck = fread(inBuffs[bSetNb], 1, blockSize, finput);
            if (sizeCheck != blockSize)
                END_PROCESS(63, "Read error : cannot access compressed block !");
            /* push to decoding thread */
            {   LegacyBlockInput* const lbi = (LegacyBlockInput*)malloc(sizeof(*lbi));
                if (lbi==NULL)
                    END_PROCESS(64, "Allocation error : not enough memory to allocate job descriptor");
                lbi->inBuffer = inBuffs[bSetNb];
                lbi->inSize = blockSize;
                lbi->outBuffer = outBuffs[bSetNb];
                lbi->wPool = wPool;
                lbi->totalSize = &streamSize;
                lbi->foutput = foutput;
                lbi->sparseEnable = prefs->sparseFileSupport;
                lbi->storedSkips = &storedSkips;
                TPool_submitJob(tPool, LZ4IO_decompressBlockLegacy, lbi);
            }
        }
    }
    if (ferror(finput)) END_PROCESS(65, "Read error : ferror");

    /* Wait for all completion */
    TPool_jobsCompleted(tPool);
    TPool_jobsCompleted(wPool);

    /* flush last zeroes */
    LZ4IO_fwriteSparseEnd(foutput, storedSkips);

    /* Free */
    TPool_free(wPool);
    TPool_free(tPool);
    for (bSetNb=0; bSetNb<NB_BUFFSETS; bSetNb++) {
        free(inBuffs[bSetNb]);
        free(outBuffs[bSetNb]);
    }

    return streamSize;
}

#else

static unsigned long long
LZ4IO_decodeLegacyStream(FILE* finput, FILE* foutput, const LZ4IO_prefs_t* prefs)
{
    unsigned long long streamSize = 0;
    unsigned storedSkips = 0;

    /* Allocate Memory */
    char* const in_buff  = (char*)malloc((size_t)LZ4_compressBound(LEGACY_BLOCKSIZE));
    char* const out_buff = (char*)malloc(LEGACY_BLOCKSIZE);
    if (!in_buff || !out_buff) END_PROCESS(61, "Allocation error : not enough memory");

    /* Main Loop */
    while (1) {
        unsigned int blockSize;

        /* Block Size */
        {   size_t const sizeCheck = fread(in_buff, 1, LZ4IO_LEGACY_BLOCK_HEADER_SIZE, finput);
            if (sizeCheck == 0) break;                   /* Nothing to read : file read is completed */
            if (sizeCheck != LZ4IO_LEGACY_BLOCK_HEADER_SIZE)
                END_PROCESS(62, "Error: cannot read block size in Legacy format");
        }
        blockSize = LZ4IO_readLE32(in_buff);       /* Convert to Little Endian */
        if (blockSize > LZ4_COMPRESSBOUND(LEGACY_BLOCKSIZE)) {
            /* Cannot read next block : maybe new stream ? */
            g_magicRead = blockSize;
            break;
        }

        /* Read Block */
        { size_t const sizeCheck = fread(in_buff, 1, blockSize, finput);
          if (sizeCheck != blockSize) END_PROCESS(63, "Read error : cannot access compressed block !"); }

        /* Decode Block */
        {   int const decodeSize = LZ4_decompress_safe(in_buff, out_buff, (int)blockSize, LEGACY_BLOCKSIZE);
            if (decodeSize < 0) END_PROCESS(64, "Decoding Failed ! Corrupted input detected !");
            streamSize += (unsigned long long)decodeSize;
            /* Write Block */
            storedSkips = LZ4IO_fwriteSparse(foutput, out_buff, (size_t)decodeSize, prefs->sparseFileSupport, storedSkips); /* success or die */
    }   }
    if (ferror(finput)) END_PROCESS(65, "Read error : ferror");

    LZ4IO_fwriteSparseEnd(foutput, storedSkips);

    /* Free */
    free(in_buff);
    free(out_buff);

    return streamSize;
}
#endif


typedef struct {
    void*  srcBuffer;
    size_t srcBufferSize;
    void*  dstBuffer;
    size_t dstBufferSize;
    FILE*  dstFile;
    LZ4F_decompressionContext_t dCtx;
    void*  dictBuffer;
    size_t dictBufferSize;
} dRess_t;

static void LZ4IO_loadDDict(dRess_t* ress, const LZ4IO_prefs_t* const prefs)
{
    if (!prefs->useDictionary) {
        ress->dictBuffer = NULL;
        ress->dictBufferSize = 0;
        return;
    }

    ress->dictBuffer = LZ4IO_createDict(&ress->dictBufferSize, prefs->dictionaryFilename);
    if (!ress->dictBuffer) END_PROCESS(25, "Dictionary error : could not create dictionary");
}

static const size_t LZ4IO_dBufferSize = 64 KB;
static dRess_t LZ4IO_createDResources(const LZ4IO_prefs_t* const prefs)
{
    dRess_t ress;

    /* init */
    LZ4F_errorCode_t const errorCode = LZ4F_createDecompressionContext(&ress.dCtx, LZ4F_VERSION);
    if (LZ4F_isError(errorCode)) END_PROCESS(60, "Can't create LZ4F context : %s", LZ4F_getErrorName(errorCode));

    /* Allocate Memory */
    ress.srcBufferSize = LZ4IO_dBufferSize;
    ress.srcBuffer = malloc(ress.srcBufferSize);
    ress.dstBufferSize = LZ4IO_dBufferSize;
    ress.dstBuffer = malloc(ress.dstBufferSize);
    if (!ress.srcBuffer || !ress.dstBuffer) END_PROCESS(61, "Allocation error : not enough memory");

    LZ4IO_loadDDict(&ress, prefs);

    ress.dstFile = NULL;
    return ress;
}

static void LZ4IO_freeDResources(dRess_t ress)
{
    LZ4F_errorCode_t errorCode = LZ4F_freeDecompressionContext(ress.dCtx);
    if (LZ4F_isError(errorCode)) END_PROCESS(69, "Error : can't free LZ4F context resource : %s", LZ4F_getErrorName(errorCode));
    free(ress.srcBuffer);
    free(ress.dstBuffer);
    free(ress.dictBuffer);
}


#if LZ4IO_MULTITHREAD

#define INBUFF_SIZE (4 MB)
#define OUTBUFF_SIZE (1 * INBUFF_SIZE)
#define OUTBUFF_QUEUE 1
#define PBUFFERS_NB (1 /* being decompressed */ + OUTBUFF_QUEUE + 1 /* being written to io */)

typedef struct {
    void* ptr;
    size_t capacity;
    size_t size;
} Buffer;

/* BufferPool:
 * Based on ayncio property :
 * all buffers are allocated and released in order,
 * maximum nb of buffers limited by queues */

typedef struct {
    Buffer buffers[PBUFFERS_NB];
    int availNext;
    int usedIdx;
} BufferPool;

static void LZ4IO_freeBufferPool(BufferPool* bp)
{
    int i;
    if (bp==NULL) return;
    for (i=0; i<PBUFFERS_NB; i++)
        free(bp->buffers[i].ptr);
    free(bp);
}

static BufferPool* LZ4IO_createBufferPool(size_t bufSize)
{
    BufferPool* const bp = (BufferPool*)calloc(1, sizeof(*bp));
    int i;
    if (bp==NULL) return NULL;
    for (i=0; i<PBUFFERS_NB; i++) {
        bp->buffers[i].ptr = malloc(bufSize);
        if (bp->buffers[i].ptr == NULL) {
            LZ4IO_freeBufferPool(bp);
            return NULL;
        }
         bp->buffers[i].capacity = bufSize;
         bp->buffers[i].size = 0;
    }
    bp->availNext = 0;
    bp->usedIdx = 0;
    return bp;
}

/* Note: Thread Sanitizer can be detected with below macro
 * but it's not guaranteed (doesn't seem to work with clang) */
#ifdef __SANITIZE_THREAD__
# undef LZ4IO_NO_TSAN_ONLY
#endif

static Buffer BufPool_getBuffer(BufferPool* bp)
{
    assert(bp != NULL);
#ifdef LZ4IO_NO_TSAN_ONLY
    /* The following assert() are susceptible to race conditions */
    assert(bp->availNext >= bp->usedIdx);
    assert(bp->availNext < bp->usedIdx + PBUFFERS_NB);
#endif
    {   int id = bp->availNext++ % PBUFFERS_NB;
        assert(bp->buffers[id].size == 0);
        return bp->buffers[id];
}   }

void BufPool_releaseBuffer(BufferPool* bp, Buffer buf)
{
    assert(bp != NULL);
#ifdef LZ4IO_NO_TSAN_ONLY
    /* The following assert() is susceptible to race conditions */
    assert(bp->usedIdx < bp->availNext);
#endif
    {   int id = bp->usedIdx++ % PBUFFERS_NB;
        assert(bp->buffers[id].ptr == buf.ptr);
        bp->buffers[id].size = 0;
}   }

typedef struct {
    Buffer bufOut;
    FILE* fOut;
    BufferPool* bp;
    int sparseEnable;
    unsigned* storedSkips;
    unsigned long long* totalSize;
} LZ4FChunkToWrite;

static void LZ4IO_writeDecodedLZ4FChunk(void* arg)
{
    LZ4FChunkToWrite* const ctw = (LZ4FChunkToWrite*)arg;
    assert(ctw != NULL);

    /* note: works because only 1 thread */
    *ctw->storedSkips = LZ4IO_fwriteSparse(ctw->fOut, ctw->bufOut.ptr, ctw->bufOut.size, ctw->sparseEnable, *ctw->storedSkips); /* success or die */
    *ctw->totalSize += (unsigned long long)ctw->bufOut.size; /* note: works because only 1 thread */
    DISPLAYUPDATE(2, "\rDecompressed : %u MiB  ", (unsigned)(ctw->totalSize[0] >> 20));

    /* clean up */
    BufPool_releaseBuffer(ctw->bp, ctw->bufOut);
    free(ctw);
}

typedef struct {
    LZ4F_dctx* dctx;
    const void* inBuffer;
    size_t inSize;
    const void* dictBuffer;
    size_t dictBufferSize;
    BufferPool* bp;
    unsigned long long* totalSize;
    LZ4F_errorCode_t* lastStatus;
    TPool* wPool;
    FILE* foutput;
    int sparseEnable;
    unsigned* storedSkips;
} LZ4FChunk;

static void LZ4IO_decompressLZ4FChunk(void* arg)
{
    LZ4FChunk* const lz4fc = (LZ4FChunk*)arg;
    const char* inPtr = (const char*)lz4fc->inBuffer;
    size_t pos = 0;

    while ((pos < lz4fc->inSize)) {  /* still to read */
        size_t remainingInSize = lz4fc->inSize - pos;
        Buffer b = BufPool_getBuffer(lz4fc->bp);
        if (b.capacity != OUTBUFF_SIZE)
            END_PROCESS(33, "Could not allocate output buffer!");
        assert(b.size == 0);
        b.size = b.capacity;
        {   size_t nextToLoad = LZ4F_decompress_usingDict(lz4fc->dctx,
                                b.ptr, &b.size,
                                inPtr + pos, &remainingInSize,
                                lz4fc->dictBuffer, lz4fc->dictBufferSize,
                                NULL);
            if (LZ4F_isError(nextToLoad))
                END_PROCESS(34, "Decompression error : %s", LZ4F_getErrorName(nextToLoad));
            *lz4fc->lastStatus = nextToLoad;
        }
        assert(remainingInSize <= lz4fc->inSize - pos);
        pos += remainingInSize;
        assert(b.size <= b.capacity);

        /* push to write thread */
        {   LZ4FChunkToWrite* const ctw = (LZ4FChunkToWrite*)malloc(sizeof(*ctw));
            if (ctw==NULL) {
                END_PROCESS(35, "Allocation error : can't describe new write job");
            }
            ctw->bufOut = b;
            ctw->fOut = lz4fc->foutput;
            ctw->bp = lz4fc->bp;
            ctw->sparseEnable = lz4fc->sparseEnable;
            ctw->storedSkips = lz4fc->storedSkips;
            ctw->totalSize = lz4fc->totalSize;
            TPool_submitJob(lz4fc->wPool, LZ4IO_writeDecodedLZ4FChunk, ctw);
        }
    }

    /* clean up */
    free(lz4fc);
}

static unsigned long long
LZ4IO_decompressLZ4F(dRess_t ress,
                     FILE* const srcFile, FILE* const dstFile,
                     const LZ4IO_prefs_t* const prefs)
{
    unsigned long long filesize = 0;
    LZ4F_errorCode_t nextToLoad;
    LZ4F_errorCode_t lastStatus = 1;
    unsigned storedSkips = 0;
    LZ4F_decompressOptions_t const dOpt_skipCrc = { 0, 1, 0, 0 };
    const LZ4F_decompressOptions_t* const dOptPtr =
        ((prefs->blockChecksum==0) && (prefs->streamChecksum==0)) ?
        &dOpt_skipCrc : NULL;
    TPool* const tPool = TPool_create(1, 1);
    TPool* const wPool = TPool_create(1, 1);
    BufferPool* const bp = LZ4IO_createBufferPool(OUTBUFF_SIZE);
#define NB_BUFFSETS 4 /* 1 being read, 1 being processed, 1 being written, 1 being queued */
    void* inBuffs[NB_BUFFSETS];
    int bSetNb;

    /* checks */
    if (tPool == NULL || wPool == NULL || bp==NULL)
        END_PROCESS(22, "threadpool creation error ");

    /* allocate buffers up front */
    for (bSetNb=0; bSetNb<NB_BUFFSETS; bSetNb++) {
        inBuffs[bSetNb] = malloc((size_t)INBUFF_SIZE);
        if (!inBuffs[bSetNb])
            END_PROCESS(23, "Allocation error : can't allocate buffer for legacy decoding");
    }

    /* Init feed with magic number (already consumed from FILE* sFile) */
    {   size_t inSize = MAGICNUMBER_SIZE;
        size_t outSize= 0;
        LZ4IO_writeLE32(ress.srcBuffer, LZ4IO_MAGICNUMBER);
        nextToLoad = LZ4F_decompress_usingDict(ress.dCtx,
                            ress.dstBuffer, &outSize,
                            ress.srcBuffer, &inSize,
                            ress.dictBuffer, ress.dictBufferSize,
                            dOptPtr);  /* set it once, it's enough */
        if (LZ4F_isError(nextToLoad))
            END_PROCESS(23, "Header error : %s", LZ4F_getErrorName(nextToLoad));
    }

    /* Main Loop */
    assert(nextToLoad);
    for (bSetNb = 0; ; bSetNb = (bSetNb+1) % NB_BUFFSETS) {
        size_t readSize;

        /* Read input */
        readSize = fread(inBuffs[bSetNb], 1, INBUFF_SIZE, srcFile);
        if (ferror(srcFile)) END_PROCESS(26, "Read error");

        /* push to decoding thread */
        {   LZ4FChunk* const lbi = (LZ4FChunk*)malloc(sizeof(*lbi));
            if (lbi==NULL)
                END_PROCESS(25, "Allocation error : not enough memory to allocate job descriptor");
            lbi->dctx = ress.dCtx;
            lbi->inBuffer = inBuffs[bSetNb];
            lbi->inSize = readSize;
            lbi->dictBuffer = ress.dictBuffer;
            lbi->dictBufferSize = ress.dictBufferSize;
            lbi->bp = bp;
            lbi->wPool = wPool;
            lbi->totalSize = &filesize;
            lbi->lastStatus = &lastStatus;
            lbi->foutput = dstFile;
            lbi->sparseEnable = prefs->sparseFileSupport;
            lbi->storedSkips = &storedSkips;
            TPool_submitJob(tPool, LZ4IO_decompressLZ4FChunk, lbi);
        }
        if (readSize < INBUFF_SIZE) break;   /* likely reached end of stream */
    }
    assert(feof(srcFile));

    /* Wait for all decompression completion */
    TPool_jobsCompleted(tPool);

    /* flush */
    if (lastStatus != 0) {
        END_PROCESS(26, "LZ4F frame decoding could not complete: invalid frame");
    }
    TPool_jobsCompleted(wPool);
    if (!prefs->testMode) LZ4IO_fwriteSparseEnd(dstFile, storedSkips);

    /* Clean */
    for (bSetNb=0; bSetNb<NB_BUFFSETS; bSetNb++) {
        free(inBuffs[bSetNb]);
    }
    LZ4IO_freeBufferPool(bp);
    TPool_free(wPool);
    TPool_free(tPool);

    return filesize;
}

#else

static unsigned long long
LZ4IO_decompressLZ4F(dRess_t ress,
                     FILE* const srcFile, FILE* const dstFile,
                     const LZ4IO_prefs_t* const prefs)
{
    unsigned long long filesize = 0;
    LZ4F_errorCode_t nextToLoad;
    unsigned storedSkips = 0;
    LZ4F_decompressOptions_t const dOpt_skipCrc = { 0, 1, 0, 0 };
    const LZ4F_decompressOptions_t* const dOptPtr =
        ((prefs->blockChecksum==0) && (prefs->streamChecksum==0)) ?
        &dOpt_skipCrc : NULL;

    /* Init feed with magic number (already consumed from FILE* sFile) */
    {   size_t inSize = MAGICNUMBER_SIZE;
        size_t outSize= 0;
        LZ4IO_writeLE32(ress.srcBuffer, LZ4IO_MAGICNUMBER);
        nextToLoad = LZ4F_decompress_usingDict(ress.dCtx,
                            ress.dstBuffer, &outSize,
                            ress.srcBuffer, &inSize,
                            ress.dictBuffer, ress.dictBufferSize,
                            dOptPtr);  /* set it once, it's enough */
        if (LZ4F_isError(nextToLoad))
            END_PROCESS(62, "Header error : %s", LZ4F_getErrorName(nextToLoad));
    }

    /* Main Loop */
    for (;nextToLoad;) {
        size_t readSize;
        size_t pos = 0;
        size_t decodedBytes = ress.dstBufferSize;

        /* Read input */
        if (nextToLoad > ress.srcBufferSize) nextToLoad = ress.srcBufferSize;
        readSize = fread(ress.srcBuffer, 1, nextToLoad, srcFile);
        if (!readSize) break;   /* reached end of file or stream */

        while ((pos < readSize) || (decodedBytes == ress.dstBufferSize)) {  /* still to read, or still to flush */
            /* Decode Input (at least partially) */
            size_t remaining = readSize - pos;
            decodedBytes = ress.dstBufferSize;
            nextToLoad = LZ4F_decompress_usingDict(ress.dCtx,
                                    ress.dstBuffer, &decodedBytes,
                                    (char*)(ress.srcBuffer)+pos, &remaining,
                                    ress.dictBuffer, ress.dictBufferSize,
                                    NULL);
            if (LZ4F_isError(nextToLoad))
                END_PROCESS(66, "Decompression error : %s", LZ4F_getErrorName(nextToLoad));
            pos += remaining;

            /* Write Block */
            if (decodedBytes) {
                if (!prefs->testMode)
                    storedSkips = LZ4IO_fwriteSparse(dstFile, ress.dstBuffer, decodedBytes, prefs->sparseFileSupport, storedSkips);
                filesize += decodedBytes;
                DISPLAYUPDATE(2, "\rDecompressed : %u MiB  ", (unsigned)(filesize>>20));
            }

            if (!nextToLoad) break;
        }
    }
    /* can be out because readSize == 0, which could be an fread() error */
    if (ferror(srcFile)) END_PROCESS(67, "Read error");

    if (!prefs->testMode) LZ4IO_fwriteSparseEnd(dstFile, storedSkips);
    if (nextToLoad!=0)
        END_PROCESS(68, "Unfinished stream (nextToLoad=%u)", (unsigned)nextToLoad);

    return filesize;
}

#endif /* LZ4IO_MULTITHREAD */

/* LZ4IO_passThrough:
 * just output the same content as input, no decoding.
 * This is a capability of zcat, and by extension lz4cat
 * MNstore : contain the first MAGICNUMBER_SIZE bytes already read from finput
 */
#define PTSIZE  (64 KB)
#define PTSIZET (PTSIZE / sizeof(size_t))
static unsigned long long
LZ4IO_passThrough(FILE* finput, FILE* foutput,
                  unsigned char MNstore[MAGICNUMBER_SIZE],
                  int sparseFileSupport)
{
	size_t buffer[PTSIZET];
    size_t readBytes = 1;
    unsigned long long total = MAGICNUMBER_SIZE;
    unsigned storedSkips = 0;

    if (fwrite(MNstore, 1, MAGICNUMBER_SIZE, foutput) != MAGICNUMBER_SIZE) {
        END_PROCESS(50, "Pass-through write error");
    }
    while (readBytes) {
        readBytes = fread(buffer, 1, sizeof(buffer), finput);
        total += readBytes;
        storedSkips = LZ4IO_fwriteSparse(foutput, buffer, readBytes, sparseFileSupport, storedSkips);
    }
    if (ferror(finput)) END_PROCESS(51, "Read Error");

    LZ4IO_fwriteSparseEnd(foutput, storedSkips);
    return total;
}

/* when fseek() doesn't work (pipe scenario),
 * read and forget from input.
**/
#define SKIP_BUFF_SIZE (16 KB)
static int skipStream(FILE* f, unsigned offset)
{
    char buf[SKIP_BUFF_SIZE];
    while (offset > 0) {
        size_t const tr = MIN(offset, sizeof(buf));
        size_t const r = fread(buf, 1, tr, f);
        if (r != tr) return 1; /* error reading f */
        offset -= (unsigned)tr;
    }
    assert(offset == 0);
    return 0;
}

/** Safely handle cases when (unsigned)offset > LONG_MAX */
static int fseek_u32(FILE *fp, unsigned offset, int where)
{
    const unsigned stepMax = 1U << 30;
    int errorNb = 0;

    if (where != SEEK_CUR) return -1;  /* Only allows SEEK_CUR */
    while (offset > 0) {
        unsigned s = offset;
        if (s > stepMax) s = stepMax;
        errorNb = UTIL_fseek(fp, (long)s, SEEK_CUR);
        if (errorNb==0) { offset -= s; continue; }
        errorNb = skipStream(fp, offset);
        offset = 0;
    }
    return errorNb;
}


#define ENDOFSTREAM ((unsigned long long)-1)
#define DECODING_ERROR ((unsigned long long)-2)
static unsigned long long
selectDecoder(dRess_t ress,
              FILE* finput, FILE* foutput,
              const LZ4IO_prefs_t* const prefs)
{
    unsigned char MNstore[MAGICNUMBER_SIZE];
    unsigned magicNumber;
    static unsigned nbFrames = 0;

    /* init */
    nbFrames++;

    /* Check Archive Header */
    if (g_magicRead) {  /* magic number already read from finput (see legacy frame)*/
        magicNumber = g_magicRead;
        g_magicRead = 0;
    } else {
        size_t const nbReadBytes = fread(MNstore, 1, MAGICNUMBER_SIZE, finput);
        if (nbReadBytes==0) { nbFrames = 0; return ENDOFSTREAM; }   /* EOF */
        if (nbReadBytes != MAGICNUMBER_SIZE)
          END_PROCESS(40, "Unrecognized header : Magic Number unreadable");
        magicNumber = LZ4IO_readLE32(MNstore);   /* Little Endian format */
    }
    if (LZ4IO_isSkippableMagicNumber(magicNumber))
        magicNumber = LZ4IO_SKIPPABLE0;   /* fold skippable magic numbers */

    switch(magicNumber)
    {
    case LZ4IO_MAGICNUMBER:
        return LZ4IO_decompressLZ4F(ress, finput, foutput, prefs);
    case LEGACY_MAGICNUMBER:
        DISPLAYLEVEL(4, "Detected : Legacy format \n");
        return LZ4IO_decodeLegacyStream(finput, foutput, prefs);
    case LZ4IO_SKIPPABLE0:
        DISPLAYLEVEL(4, "Skipping detected skippable area \n");
        {   size_t const nbReadBytes = fread(MNstore, 1, 4, finput);
            if (nbReadBytes != 4)
                END_PROCESS(42, "Stream error : skippable size unreadable");
        }
        {   unsigned const size = LZ4IO_readLE32(MNstore);
            int const errorNb = fseek_u32(finput, size, SEEK_CUR);
            if (errorNb != 0)
                END_PROCESS(43, "Stream error : cannot skip skippable area");
        }
        return 0;
    default:
        if (nbFrames == 1) {  /* just started */
            /* Wrong magic number at the beginning of 1st stream */
            if (!prefs->testMode && prefs->overwrite && prefs->passThrough) {
                nbFrames = 0;
                return LZ4IO_passThrough(finput, foutput, MNstore, prefs->sparseFileSupport);
            }
            END_PROCESS(44,"Unrecognized header : file cannot be decoded");
        }
        {   long int const position = ftell(finput);  /* only works for files < 2 GB */
            DISPLAYLEVEL(2, "Stream followed by undecodable data ");
            if (position != -1L)
                DISPLAYLEVEL(2, "at position %i ", (int)position);
            DISPLAYLEVEL(2, "\n");
        }
        return DECODING_ERROR;
    }
}


static int
LZ4IO_decompressSrcFile(unsigned long long* outGenSize,
                        dRess_t ress,
                        const char* input_filename, const char* output_filename,
                        const LZ4IO_prefs_t* const prefs)
{
    FILE* const foutput = ress.dstFile;
    unsigned long long filesize = 0;
    int result = 0;

    /* Init */
    FILE* const finput = LZ4IO_openSrcFile(input_filename);
    if (finput==NULL) return 1;
    assert(foutput != NULL);

    /* Loop over multiple streams */
    for ( ; ; ) {  /* endless loop, see break condition */
        unsigned long long const decodedSize =
                        selectDecoder(ress, finput, foutput, prefs);
        if (decodedSize == ENDOFSTREAM) break;
        if (decodedSize == DECODING_ERROR) { result=1; break; }
        filesize += decodedSize;
    }

    /* Close input */
    fclose(finput);
    if (prefs->removeSrcFile) {  /* --rm */
        if (remove(input_filename))
            END_PROCESS(45, "Remove error : %s: %s", input_filename, strerror(errno));
    }

    /* Final Status */
    DISPLAYLEVEL(2, "\r%79s\r", "");
    DISPLAYLEVEL(2, "%-30.30s : decoded %llu bytes \n", input_filename, filesize);
    *outGenSize = filesize;
    (void)output_filename;

    return result;
}


static int
LZ4IO_decompressDstFile(unsigned long long* outGenSize,
                        dRess_t ress,
                        const char* input_filename,
                        const char* output_filename,
                        const LZ4IO_prefs_t* const prefs)
{
    int result;
    stat_t statbuf;
    int stat_result = 0;
    FILE* const foutput = LZ4IO_openDstFile(output_filename, prefs);
    if (foutput==NULL) return 1;   /* failure */

    if ( !LZ4IO_isStdin(input_filename)
      && UTIL_getFileStat(input_filename, &statbuf))
        stat_result = 1;

    ress.dstFile = foutput;
    result = LZ4IO_decompressSrcFile(outGenSize, ress, input_filename, output_filename, prefs);

    fclose(foutput);

    /* Copy owner, file permissions and modification time */
    if ( stat_result != 0
      && !LZ4IO_isStdout(output_filename)
      && !LZ4IO_isDevNull(output_filename)) {
        UTIL_setFileStat(output_filename, &statbuf);
        /* should return value be read ? or is silent fail good enough ? */
    }

    return result;
}


/* Note : LZ4IO_decompressFilename()
 * can provide total decompression time for the specified fileName.
 * This information is not available with LZ4IO_decompressMultipleFilenames().
 */
int LZ4IO_decompressFilename(const char* input_filename, const char* output_filename, const LZ4IO_prefs_t* prefs)
{
    dRess_t const ress = LZ4IO_createDResources(prefs);
    TIME_t const timeStart = TIME_getTime();
    double const cpuStart = cpuTime_sec();
    unsigned long long processed = 0;

    int const errStat = LZ4IO_decompressDstFile(&processed, ress, input_filename, output_filename, prefs);
    if (errStat)
        LZ4IO_finalTimeDisplay(timeStart, cpuStart, processed);
    LZ4IO_freeDResources(ress);
    return errStat;
}


int LZ4IO_decompressMultipleFilenames(
                            const char** inFileNamesTable, int ifntSize,
                            const char* suffix,
                            const LZ4IO_prefs_t* prefs)
{
    int i;
    unsigned long long totalProcessed = 0;
    int skippedFiles = 0;
    int missingFiles = 0;
    char* outFileName = (char*)malloc(FNSPACE);
    size_t ofnSize = FNSPACE;
    size_t const suffixSize = strlen(suffix);
    dRess_t ress = LZ4IO_createDResources(prefs);
    TIME_t timeStart = TIME_getTime();
    double const cpuStart = cpuTime_sec();

    if (outFileName==NULL) END_PROCESS(70, "Memory allocation error");
    if (prefs->blockChecksum==0 && prefs->streamChecksum==0) {
        DISPLAYLEVEL(4, "disabling checksum validation during decoding \n");
    }
    ress.dstFile = LZ4IO_openDstFile(stdoutmark, prefs);

    for (i=0; i<ifntSize; i++) {
        unsigned long long processed = 0;
        size_t const ifnSize = strlen(inFileNamesTable[i]);
        const char* const suffixPtr = inFileNamesTable[i] + ifnSize - suffixSize;
        if (LZ4IO_isStdout(suffix) || LZ4IO_isDevNull(suffix)) {
            missingFiles += LZ4IO_decompressSrcFile(&processed, ress, inFileNamesTable[i], suffix, prefs);
            totalProcessed += processed;
            continue;
        }
        if (ofnSize <= ifnSize-suffixSize+1) {
            free(outFileName);
            ofnSize = ifnSize + 20;
            outFileName = (char*)malloc(ofnSize);
            if (outFileName==NULL) END_PROCESS(71, "Memory allocation error");
        }
        if (ifnSize <= suffixSize  || !UTIL_sameString(suffixPtr, suffix) ) {
            DISPLAYLEVEL(1, "File extension doesn't match expected LZ4_EXTENSION (%4s); will not process file: %s\n", suffix, inFileNamesTable[i]);
            skippedFiles++;
            continue;
        }
        memcpy(outFileName, inFileNamesTable[i], ifnSize - suffixSize);
        outFileName[ifnSize-suffixSize] = '\0';
        missingFiles += LZ4IO_decompressDstFile(&processed, ress, inFileNamesTable[i], outFileName, prefs);
        totalProcessed += processed;
    }

    LZ4IO_freeDResources(ress);
    free(outFileName);
    LZ4IO_finalTimeDisplay(timeStart, cpuStart, totalProcessed);
    return missingFiles + skippedFiles;
}


/* ********************************************************************* */
/* **********************   LZ4 --list command   *********************** */
/* ********************************************************************* */

typedef enum
{
    lz4Frame = 0,
    legacyFrame,
    skippableFrame
} LZ4IO_frameType_t;

typedef struct {
    LZ4F_frameInfo_t lz4FrameInfo;
    LZ4IO_frameType_t frameType;
} LZ4IO_frameInfo_t;

#define LZ4IO_INIT_FRAMEINFO  { LZ4F_INIT_FRAMEINFO, lz4Frame }

typedef struct {
    const char* fileName;
    unsigned long long fileSize;
    unsigned long long frameCount;
    LZ4IO_frameInfo_t frameSummary;
    unsigned short eqFrameTypes;
    unsigned short eqBlockTypes;
    unsigned short allContentSize;
} LZ4IO_cFileInfo_t;

#define LZ4IO_INIT_CFILEINFO  { NULL, 0ULL, 0, LZ4IO_INIT_FRAMEINFO, 1, 1, 1 }

typedef enum { LZ4IO_LZ4F_OK, LZ4IO_format_not_known, LZ4IO_not_a_file } LZ4IO_infoResult;

static const char * LZ4IO_frameTypeNames[] = {"LZ4Frame", "LegacyFrame", "SkippableFrame" };

/* Read block headers and skip block data
   Return total blocks size for this frame including block headers,
   block checksums and content checksums.
   returns 0 in case it can't successfully skip block data.
   Assumes SEEK_CUR after frame header.
 */
static unsigned long long
LZ4IO_skipBlocksData(FILE* finput,
               const LZ4F_blockChecksum_t blockChecksumFlag,
               const LZ4F_contentChecksum_t contentChecksumFlag)
{
    unsigned char blockInfo[LZ4F_BLOCK_HEADER_SIZE];
    unsigned long long totalBlocksSize = 0;
    for (;;) {
        if (!fread(blockInfo, 1, LZ4F_BLOCK_HEADER_SIZE, finput)) {
            if (feof(finput)) return totalBlocksSize;
            return 0;
        }
        totalBlocksSize += LZ4F_BLOCK_HEADER_SIZE;
        {   const unsigned long nextCBlockSize = LZ4IO_readLE32(&blockInfo) & 0x7FFFFFFFU;
            const unsigned long nextBlock = nextCBlockSize + (blockChecksumFlag * LZ4F_BLOCK_CHECKSUM_SIZE);
            if (nextCBlockSize == 0) {
                /* Reached EndMark */
                if (contentChecksumFlag) {
                    /* Skip content checksum */
                    if (UTIL_fseek(finput, LZ4F_CONTENT_CHECKSUM_SIZE, SEEK_CUR) != 0) {
                        return 0;
                    }
                    totalBlocksSize += LZ4F_CONTENT_CHECKSUM_SIZE;
                }
                break;
            }
            totalBlocksSize += nextBlock;
            /* skip to the next block */
            assert(nextBlock < LONG_MAX);
            if (UTIL_fseek(finput, (long)nextBlock, SEEK_CUR) != 0) return 0;
    }   }
    return totalBlocksSize;
}

static const unsigned long long legacyFrameUndecodable = (0ULL-1);
/* For legacy frames only.
   Read block headers and skip block data.
   Return total blocks size for this frame including block headers.
   or legacyFrameUndecodable in case it can't successfully skip block data.
   This works as long as legacy block header size = magic number size.
   Assumes SEEK_CUR after frame header.
 */
static unsigned long long LZ4IO_skipLegacyBlocksData(FILE* finput)
{
    unsigned char blockInfo[LZ4IO_LEGACY_BLOCK_HEADER_SIZE];
    unsigned long long totalBlocksSize = 0;
    LZ4IO_STATIC_ASSERT(LZ4IO_LEGACY_BLOCK_HEADER_SIZE == MAGICNUMBER_SIZE);
    for (;;) {
        size_t const bhs = fread(blockInfo, 1, LZ4IO_LEGACY_BLOCK_HEADER_SIZE, finput);
        if (bhs == 0) {
            if (feof(finput)) return totalBlocksSize;
            return legacyFrameUndecodable;
        }
        if (bhs != 4) {
            return legacyFrameUndecodable;
        }
        {   const unsigned int nextCBlockSize = LZ4IO_readLE32(&blockInfo);
            if ( nextCBlockSize == LEGACY_MAGICNUMBER
              || nextCBlockSize == LZ4IO_MAGICNUMBER
              || LZ4IO_isSkippableMagicNumber(nextCBlockSize) ) {
                /* Rewind back. we want cursor at the beginning of next frame */
                if (UTIL_fseek(finput, -LZ4IO_LEGACY_BLOCK_HEADER_SIZE, SEEK_CUR) != 0) {
                    END_PROCESS(37, "impossible to skip backward");
                }
                break;
            }
            if (nextCBlockSize > LZ4IO_LEGACY_BLOCK_SIZE_MAX) {
                DISPLAYLEVEL(4, "Error : block in legacy frame is too large \n");
                return legacyFrameUndecodable;
            }
            totalBlocksSize += LZ4IO_LEGACY_BLOCK_HEADER_SIZE + nextCBlockSize;
            /* skip to the next block
             * note : this won't fail if nextCBlockSize is too large, skipping past the end of finput */
            if (UTIL_fseek(finput, nextCBlockSize, SEEK_CUR) != 0) {
                return legacyFrameUndecodable;
    }   }   }
    return totalBlocksSize;
}

/* LZ4IO_blockTypeID:
 * return human-readable block type, following command line convention
 * buffer : must be a valid memory area of at least 4 bytes */
const char* LZ4IO_blockTypeID(LZ4F_blockSizeID_t sizeID, LZ4F_blockMode_t blockMode, char buffer[4])
{
    buffer[0] = 'B';
    assert(sizeID >= 4); assert(sizeID <= 7);
    buffer[1] = (char)(sizeID + '0');
    buffer[2] = (blockMode == LZ4F_blockIndependent) ? 'I' : 'D';
    buffer[3] = 0;
    return buffer;
}

/* buffer : must be valid memory area of at least 10 bytes */
static const char* LZ4IO_toHuman(long double size, char* buf)
{
    const char units[] = {"\0KMGTPEZY"};
    size_t i = 0;
    for (; size >= 1024; i++) size /= 1024;
    sprintf(buf, "%.2Lf%c", size, units[i]);
    return buf;
}

/* Get filename without path prefix */
static const char* LZ4IO_baseName(const char* input_filename)
{
    const char* b = strrchr(input_filename, '/');
    if (!b) b = strrchr(input_filename, '\\');
    if (!b) return input_filename;
    return b + 1;
}

/* Report frame/s information (--list) in verbose mode (-v).
 * Will populate file info with fileName and frameSummary where applicable.
 * - TODO :
 *  + report nb of blocks, hence max. possible decompressed size (when not reported in header)
 */
static LZ4IO_infoResult
LZ4IO_getCompressedFileInfo(LZ4IO_cFileInfo_t* cfinfo, const char* input_filename, int displayNow)
{
    LZ4IO_infoResult result = LZ4IO_format_not_known;  /* default result (error) */
    unsigned char buffer[LZ4F_HEADER_SIZE_MAX];
    FILE* const finput = LZ4IO_openSrcFile(input_filename);

    if (finput == NULL) return LZ4IO_not_a_file;
    cfinfo->fileSize = UTIL_getOpenFileSize(finput);

    while (!feof(finput)) {
        LZ4IO_frameInfo_t frameInfo = LZ4IO_INIT_FRAMEINFO;
        unsigned magicNumber;
        /* Get MagicNumber */
        {   size_t const nbReadBytes = fread(buffer, 1, MAGICNUMBER_SIZE, finput);
            if (nbReadBytes == 0) { break; } /* EOF */
            result = LZ4IO_format_not_known;  /* default result (error) */
            if (nbReadBytes != MAGICNUMBER_SIZE) {
                END_PROCESS(40, "Unrecognized header : Magic Number unreadable");
        }   }
        magicNumber = LZ4IO_readLE32(buffer);   /* Little Endian format */
        if (LZ4IO_isSkippableMagicNumber(magicNumber))
            magicNumber = LZ4IO_SKIPPABLE0;   /* fold skippable magic numbers */

        switch (magicNumber) {
        case LZ4IO_MAGICNUMBER:
            if (cfinfo->frameSummary.frameType != lz4Frame) cfinfo->eqFrameTypes = 0;
            /* Get frame info */
            {   const size_t readBytes = fread(buffer + MAGICNUMBER_SIZE, 1, LZ4F_HEADER_SIZE_MIN - MAGICNUMBER_SIZE, finput);
                if (!readBytes || ferror(finput)) END_PROCESS(71, "Error reading %s", input_filename);
            }
            {   size_t hSize = LZ4F_headerSize(&buffer, LZ4F_HEADER_SIZE_MIN);
                if (LZ4F_isError(hSize)) break;
                if (hSize > (LZ4F_HEADER_SIZE_MIN + MAGICNUMBER_SIZE)) {
                    /* We've already read LZ4F_HEADER_SIZE_MIN so read any extra until hSize*/
                    const size_t readBytes = fread(buffer + LZ4F_HEADER_SIZE_MIN, 1, hSize - LZ4F_HEADER_SIZE_MIN, finput);
                    if (!readBytes || ferror(finput)) END_PROCESS(72, "Error reading %s", input_filename);
                }
                /* Create decompression context */
                {   LZ4F_dctx* dctx;
                    if ( LZ4F_isError(LZ4F_createDecompressionContext(&dctx, LZ4F_VERSION)) ) break;
                    {   unsigned const frameInfoError = LZ4F_isError(LZ4F_getFrameInfo(dctx, &frameInfo.lz4FrameInfo, buffer, &hSize));
                        LZ4F_freeDecompressionContext(dctx);
                        if (frameInfoError) break;
                        if ((cfinfo->frameSummary.lz4FrameInfo.blockSizeID != frameInfo.lz4FrameInfo.blockSizeID ||
                                cfinfo->frameSummary.lz4FrameInfo.blockMode != frameInfo.lz4FrameInfo.blockMode)
                                && cfinfo->frameCount != 0)
                            cfinfo->eqBlockTypes = 0;
                        {   const unsigned long long totalBlocksSize = LZ4IO_skipBlocksData(finput,
                                    frameInfo.lz4FrameInfo.blockChecksumFlag,
                                    frameInfo.lz4FrameInfo.contentChecksumFlag);
                            if (totalBlocksSize) {
                                char bTypeBuffer[5];
                                LZ4IO_blockTypeID(frameInfo.lz4FrameInfo.blockSizeID, frameInfo.lz4FrameInfo.blockMode, bTypeBuffer);
                                if (displayNow) DISPLAYOUT("    %6llu %14s %5s %8s",
                                             cfinfo->frameCount + 1,
                                             LZ4IO_frameTypeNames[frameInfo.frameType],
                                             bTypeBuffer,
                                             frameInfo.lz4FrameInfo.contentChecksumFlag ? "XXH32" : "-");
                                if (frameInfo.lz4FrameInfo.contentSize) {
                                    double const ratio = (double)(totalBlocksSize + hSize) / (double)frameInfo.lz4FrameInfo.contentSize * 100;
                                    if (displayNow) DISPLAYOUT(" %20llu %20llu %9.2f%%\n",
                                                    totalBlocksSize + hSize,
                                                    frameInfo.lz4FrameInfo.contentSize,
                                                    ratio);
                                    /* Now we've consumed frameInfo we can use it to store the total contentSize */
                                    frameInfo.lz4FrameInfo.contentSize += cfinfo->frameSummary.lz4FrameInfo.contentSize;
                                }
                                else {
                                    if (displayNow) DISPLAYOUT(" %20llu %20s %9s \n", totalBlocksSize + hSize, "-", "-");
                                    cfinfo->allContentSize = 0;
                                }
                                result = LZ4IO_LZ4F_OK;
            }   }   }   }   }
            break;
        case LEGACY_MAGICNUMBER:
            frameInfo.frameType = legacyFrame;
            if (cfinfo->frameSummary.frameType != legacyFrame && cfinfo->frameCount != 0) cfinfo->eqFrameTypes = 0;
            cfinfo->eqBlockTypes = 0;
            cfinfo->allContentSize = 0;
            {   const unsigned long long totalBlocksSize = LZ4IO_skipLegacyBlocksData(finput);
                if (totalBlocksSize == legacyFrameUndecodable) {
                    DISPLAYLEVEL(1, "Corrupted legacy frame \n");
                    result = LZ4IO_format_not_known;
                    break;
                }
                if (totalBlocksSize) {
                    if (displayNow) DISPLAYOUT("    %6llu %14s %5s %8s %20llu %20s %9s\n",
                                 cfinfo->frameCount + 1,
                                 LZ4IO_frameTypeNames[frameInfo.frameType],
                                 "-", "-",
                                 totalBlocksSize + 4,
                                 "-", "-");
                    result = LZ4IO_LZ4F_OK;
            }   }
            break;
        case LZ4IO_SKIPPABLE0:
            frameInfo.frameType = skippableFrame;
            if (cfinfo->frameSummary.frameType != skippableFrame && cfinfo->frameCount != 0) cfinfo->eqFrameTypes = 0;
            cfinfo->eqBlockTypes = 0;
            cfinfo->allContentSize = 0;
            {   size_t const nbReadBytes = fread(buffer, 1, 4, finput);
                if (nbReadBytes != 4)
                    END_PROCESS(42, "Stream error : skippable size unreadable");
            }
            {   unsigned const size = LZ4IO_readLE32(buffer);
                int const errorNb = fseek_u32(finput, size, SEEK_CUR);
                if (errorNb != 0)
                    END_PROCESS(43, "Stream error : cannot skip skippable area");
                if (displayNow) DISPLAYOUT("    %6llu %14s %5s %8s %20u %20s %9s\n",
                             cfinfo->frameCount + 1,
                             "SkippableFrame",
                             "-", "-", size + 8, "-", "-");

                result = LZ4IO_LZ4F_OK;
            }
            break;
        default:
            {   long int const position = ftell(finput);  /* only works for files < 2 GB */
                DISPLAYLEVEL(3, "Stream followed by undecodable data ");
                if (position != -1L)
                    DISPLAYLEVEL(3, "at position %i ", (int)position);
                result = LZ4IO_format_not_known;
                DISPLAYLEVEL(3, "\n");
            }
        break;
        }
        if (result != LZ4IO_LZ4F_OK) break;
        cfinfo->frameSummary = frameInfo;
        cfinfo->frameCount++;
    }  /* while (!feof(finput)) */
    fclose(finput);
    return result;
}


int LZ4IO_displayCompressedFilesInfo(const char** inFileNames, size_t ifnIdx)
{
    int result = 0;
    size_t idx = 0;
    if (g_displayLevel < 3) {
        DISPLAYOUT("%10s %14s %5s %11s %13s %8s   %s\n",
                "Frames", "Type", "Block", "Compressed", "Uncompressed", "Ratio", "Filename");
    }
    for (; idx < ifnIdx; idx++) {
        /* Get file info */
        LZ4IO_cFileInfo_t cfinfo = LZ4IO_INIT_CFILEINFO;
        cfinfo.fileName = LZ4IO_baseName(inFileNames[idx]);
        if (LZ4IO_isStdin(inFileNames[idx]) ? !UTIL_isRegFD(0) : !UTIL_isRegFile(inFileNames[idx])) {
            DISPLAYLEVEL(1, "lz4: %s is not a regular file \n", inFileNames[idx]);
            return 1;
        }
        if (g_displayLevel >= 3) {
            /* verbose mode */
            DISPLAYOUT("%s(%llu/%llu)\n", cfinfo.fileName, (unsigned long long)idx + 1, (unsigned  long long)ifnIdx);
            DISPLAYOUT("    %6s %14s %5s %8s %20s %20s %9s\n",
                        "Frame", "Type", "Block", "Checksum", "Compressed", "Uncompressed", "Ratio");
        }
        {   LZ4IO_infoResult const op_result = LZ4IO_getCompressedFileInfo(&cfinfo, inFileNames[idx], g_displayLevel >= 3);
            if (op_result != LZ4IO_LZ4F_OK) {
                assert(op_result == LZ4IO_format_not_known);
                DISPLAYLEVEL(1, "lz4: %s: File format not recognized \n", inFileNames[idx]);
                return 1;
        }   }
        if (g_displayLevel >= 3) {
            DISPLAYOUT("\n");
        }
        if (g_displayLevel < 3) {
            /* Display summary */
            char buffers[3][10];
            DISPLAYOUT("%10llu %14s %5s %11s %13s ",
                    cfinfo.frameCount,
                    cfinfo.eqFrameTypes ? LZ4IO_frameTypeNames[cfinfo.frameSummary.frameType] : "-" ,
                    cfinfo.eqBlockTypes ? LZ4IO_blockTypeID(cfinfo.frameSummary.lz4FrameInfo.blockSizeID,
                                                            cfinfo.frameSummary.lz4FrameInfo.blockMode, buffers[0]) : "-",
                    LZ4IO_toHuman((long double)cfinfo.fileSize, buffers[1]),
                    cfinfo.allContentSize ? LZ4IO_toHuman((long double)cfinfo.frameSummary.lz4FrameInfo.contentSize, buffers[2]) : "-");
            if (cfinfo.allContentSize) {
                double const ratio = (double)cfinfo.fileSize / (double)cfinfo.frameSummary.lz4FrameInfo.contentSize * 100;
                DISPLAYOUT("%8.2f%%  %s \n", ratio, cfinfo.fileName);
            } else {
                DISPLAYOUT("%8s   %s\n",
                        "-",
                        cfinfo.fileName);
        }   }  /* if (g_displayLevel < 3) */
    }  /* for (; idx < ifnIdx; idx++) */

    return result;
}
