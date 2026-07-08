/*
    bench.c - Demo program to benchmark open-source compression algorithms
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
    - LZ4 homepage : http://www.lz4.org
    - LZ4 source repository : https://github.com/lz4/lz4
*/


/*-************************************
*  Compiler options
**************************************/
#ifdef _MSC_VER    /* Visual Studio */
#  pragma warning(disable : 4127)    /* disable: C4127: conditional expression is constant */
#endif


/* *************************************
*  Includes
***************************************/
#include "platform.h"    /* Compiler options */
#include "util.h"        /* UTIL_GetFileSize, UTIL_sleep */
#include <stdlib.h>      /* malloc, free */
#include <string.h>      /* memset */
#include <stdio.h>       /* fprintf, fopen, ftello */
#include <time.h>        /* clock_t, clock, CLOCKS_PER_SEC */
#include <assert.h>      /* assert */

#include "lorem.h"       /* LOREM_genBuffer */
#include "xxhash.h"
#include "bench.h"
#include "timefn.h"

#define LZ4_STATIC_LINKING_ONLY
#include "lz4.h"
#define LZ4_HC_STATIC_LINKING_ONLY
#include "lz4hc.h"
#include "lz4frame.h"   /* LZ4F_decompress */


/* *************************************
*  Constants
***************************************/
#ifndef LZ4_GIT_COMMIT_STRING
#  define LZ4_GIT_COMMIT_STRING ""
#else
#  define LZ4_GIT_COMMIT_STRING LZ4_EXPAND_AND_QUOTE(LZ4_GIT_COMMIT)
#endif

#define NBSECONDS             3
#define TIMELOOP_MICROSEC     1*1000000ULL /* 1 second */
#define TIMELOOP_NANOSEC      1*1000000000ULL /* 1 second */
#define ACTIVEPERIOD_NANOSEC 70*1000000000ULL /* 70 seconds */
#define COOLPERIOD_SEC        10
#define DECOMP_MULT           1 /* test decompression DECOMP_MULT times longer than compression */

#define KB *(1 <<10)
#define MB *(1 <<20)
#define GB *(1U<<30)

#define LZ4_MAX_DICT_SIZE (64 KB)

static const size_t maxMemory = (sizeof(size_t)==4)  ?  (2 GB - 64 MB) : (size_t)(1ULL << ((sizeof(size_t)*8)-31));


/* *************************************
*  console display
***************************************/
#define DISPLAYOUT(...)      fprintf(stdout, __VA_ARGS__)
#define OUTLEVEL(l, ...)     if (g_displayLevel>=(l)) { DISPLAYOUT(__VA_ARGS__); }
#define DISPLAY(...)         fprintf(stderr, __VA_ARGS__)
#define DISPLAYLEVEL(l, ...) if (g_displayLevel>=(l)) { DISPLAY(__VA_ARGS__); }
static U32 g_displayLevel = 2;   /* 0 : no display;   1: errors;   2 : + result + interaction + warnings;   3 : + progression;   4 : + information */

#define DISPLAYUPDATE(l, ...) if (g_displayLevel>=l) { \
            if ((clock() - g_time > refreshRate) || (g_displayLevel>=4)) \
            { g_time = clock(); DISPLAY(__VA_ARGS__); \
            if (g_displayLevel>=4) fflush(stdout); } }
static const clock_t refreshRate = CLOCKS_PER_SEC * 15 / 100;
static clock_t g_time = 0;


/* *************************************
*  DEBUG and error conditions
***************************************/
#ifndef DEBUG
#  define DEBUG 0
#endif
#define DEBUGOUTPUT(...) if (DEBUG) DISPLAY(__VA_ARGS__);
#define END_PROCESS(error, ...)                                           \
do {                                                                      \
    DEBUGOUTPUT("Error defined at %s, line %i : \n", __FILE__, __LINE__); \
    DISPLAYLEVEL(1, "Error %i : ", error);                                \
    DISPLAYLEVEL(1, __VA_ARGS__);                                         \
    DISPLAYLEVEL(1, "\n");                                                \
    exit(error);                                                          \
} while (0)

#define LZ4_isError(errcode) (errcode==0)


/* *************************************
*  Benchmark Parameters
***************************************/
static U32 g_nbSeconds = NBSECONDS;
static size_t g_blockSize = 0;
int g_additionalParam = 0;
int g_benchSeparately = 0;
int g_decodeOnly = 0;
unsigned g_skipChecksums = 0;

void BMK_setNotificationLevel(unsigned level) { g_displayLevel=level; }

void BMK_setAdditionalParam(int additionalParam) { g_additionalParam=additionalParam; }

void BMK_setNbSeconds(unsigned nbSeconds)
{
    g_nbSeconds = nbSeconds;
    DISPLAYLEVEL(3, "- test >= %u seconds per compression / decompression -\n", g_nbSeconds);
}

void BMK_setBlockSize(size_t blockSize) { g_blockSize = blockSize; }

void BMK_setBenchSeparately(int separate) { g_benchSeparately = (separate!=0); }

void BMK_setDecodeOnlyMode(int set) { g_decodeOnly = (set!=0); }

void BMK_skipChecksums(int skip) { g_skipChecksums = (skip!=0); }


/* *************************************
 *  Compression state management
***************************************/

struct compressionParameters
{
    int cLevel;
    const char* dictBuf;
    int dictSize;

    LZ4_stream_t* LZ4_stream;
    LZ4_stream_t* LZ4_dictStream;
    LZ4_streamHC_t* LZ4_streamHC;
    LZ4_streamHC_t* LZ4_dictStreamHC;

    void (*initFunction)(
        struct compressionParameters* pThis);
    void (*resetFunction)(
        const struct compressionParameters* pThis);
    int (*blockFunction)(
        const struct compressionParameters* pThis,
        const char* src, char* dst, int srcSize, int dstSize);
    void (*cleanupFunction)(
        const struct compressionParameters* pThis);
};

static void
LZ4_compressInitNoStream(struct compressionParameters* pThis)
{
    pThis->LZ4_stream = NULL;
    pThis->LZ4_dictStream = NULL;
    pThis->LZ4_streamHC = NULL;
    pThis->LZ4_dictStreamHC = NULL;
}

static void
LZ4_compressInitStream(struct compressionParameters* pThis)
{
    pThis->LZ4_stream = LZ4_createStream();
    pThis->LZ4_dictStream = LZ4_createStream();
    pThis->LZ4_streamHC = NULL;
    pThis->LZ4_dictStreamHC = NULL;
    LZ4_loadDictSlow(pThis->LZ4_dictStream, pThis->dictBuf, pThis->dictSize);
}

static void
LZ4_compressInitStreamHC(struct compressionParameters* pThis)
{
    pThis->LZ4_stream = NULL;
    pThis->LZ4_dictStream = NULL;
    pThis->LZ4_streamHC = LZ4_createStreamHC();
    pThis->LZ4_dictStreamHC = LZ4_createStreamHC();
    LZ4_resetStreamHC_fast(pThis->LZ4_dictStreamHC, pThis->cLevel);
    LZ4_loadDictHC(pThis->LZ4_dictStreamHC, pThis->dictBuf, pThis->dictSize);
}

static void
LZ4_compressResetNoStream(const struct compressionParameters* cparams)
{
    (void)cparams;
}

static void
LZ4_compressResetStream(const struct compressionParameters* cparams)
{
    LZ4_resetStream_fast(cparams->LZ4_stream);
    LZ4_attach_dictionary(cparams->LZ4_stream, cparams->LZ4_dictStream);
}

static void
LZ4_compressResetStreamHC(const struct compressionParameters* cparams)
{
    LZ4_resetStreamHC_fast(cparams->LZ4_streamHC, cparams->cLevel);
    LZ4_attach_HC_dictionary(cparams->LZ4_streamHC, cparams->LZ4_dictStreamHC);
}

static int
LZ4_compressBlockNoStream(const struct compressionParameters* cparams,
                          const char* src, char* dst,
                          int srcSize, int dstSize)
{
    int const acceleration = (cparams->cLevel < 0) ? -cparams->cLevel + 1 : 1;
    return LZ4_compress_fast(src, dst, srcSize, dstSize, acceleration);
}

static int
LZ4_compressBlockNoStreamHC(const struct compressionParameters* cparams,
                            const char* src, char* dst,
                            int srcSize, int dstSize)
{
    return LZ4_compress_HC(src, dst, srcSize, dstSize, cparams->cLevel);
}

static int
LZ4_compressBlockStream(const struct compressionParameters* cparams,
                        const char* src, char* dst,
                        int srcSize, int dstSize)
{
    int const acceleration = (cparams->cLevel < 0) ? -cparams->cLevel + 1 : 1;
    LZ4_compressResetStream(cparams);
    return LZ4_compress_fast_continue(cparams->LZ4_stream, src, dst, srcSize, dstSize, acceleration);
}

static int
LZ4_compressBlockStreamHC(const struct compressionParameters* cparams,
                          const char* src, char* dst,
                          int srcSize, int dstSize)
{
    LZ4_compressResetStreamHC(cparams);
    return LZ4_compress_HC_continue(cparams->LZ4_streamHC, src, dst, srcSize, dstSize);
}

static void
LZ4_compressCleanupNoStream(const struct compressionParameters* cparams)
{
    (void)cparams;
}

static void
LZ4_compressCleanupStream(const struct compressionParameters* cparams)
{
    LZ4_freeStream(cparams->LZ4_stream);
    LZ4_freeStream(cparams->LZ4_dictStream);
}

static void
LZ4_compressCleanupStreamHC(const struct compressionParameters* cparams)
{
    LZ4_freeStreamHC(cparams->LZ4_streamHC);
    LZ4_freeStreamHC(cparams->LZ4_dictStreamHC);
}

static void
LZ4_buildCompressionParameters(struct compressionParameters* pParams,
                               int cLevel,
                         const char* dictBuf, int dictSize)
{
    pParams->cLevel = cLevel;
    pParams->dictBuf = dictBuf;
    pParams->dictSize = dictSize;

    if (dictSize) {
        if (cLevel < LZ4HC_CLEVEL_MIN) {
            pParams->initFunction = LZ4_compressInitStream;
            pParams->resetFunction = LZ4_compressResetStream;
            pParams->blockFunction = LZ4_compressBlockStream;
            pParams->cleanupFunction = LZ4_compressCleanupStream;
        } else {
            pParams->initFunction = LZ4_compressInitStreamHC;
            pParams->resetFunction = LZ4_compressResetStreamHC;
            pParams->blockFunction = LZ4_compressBlockStreamHC;
            pParams->cleanupFunction = LZ4_compressCleanupStreamHC;
        }
    } else {
        pParams->initFunction = LZ4_compressInitNoStream;
        pParams->resetFunction = LZ4_compressResetNoStream;
        pParams->cleanupFunction = LZ4_compressCleanupNoStream;

        if (cLevel < LZ4HC_CLEVEL_MIN) {
            pParams->blockFunction = LZ4_compressBlockNoStream;
        } else {
            pParams->blockFunction = LZ4_compressBlockNoStreamHC;
        }
    }
}


typedef int (*DecFunction_f)(const char* src, char* dst,
                             int srcSize, int dstCapacity,
                             const char* dictStart, int dictSize);

static LZ4F_dctx* g_dctx = NULL;

static int
LZ4F_decompress_binding(const char* src, char* dst,
                        int srcSize, int dstCapacity,
                  const char* dictStart, int dictSize)
{
    size_t dstSize = (size_t)dstCapacity;
    size_t readSize = (size_t)srcSize;
    LZ4F_decompressOptions_t dOpt = { 1, 0, 0, 0 };
    size_t decStatus;
    dOpt.skipChecksums = g_skipChecksums;
    decStatus = LZ4F_decompress(g_dctx,
                    dst, &dstSize,
                    src, &readSize,
                    &dOpt);
    if ( (decStatus == 0)   /* decompression successful */
      && ((int)readSize==srcSize) /* consume all input */ )
        return (int)dstSize;
    /* else, error */
    return -1;
    (void)dictStart; (void)dictSize;  /* not compatible with dictionary yet */
}


/* ********************************************************
*  Bench functions
**********************************************************/
typedef struct {
    const char* srcPtr;
    size_t srcSize;
    char*  cPtr;
    size_t cRoom;
    size_t cSize;
    char*  resPtr;
    size_t resSize;
} blockParam_t;

#define MIN(a,b) ((a)<(b) ? (a) : (b))
#define MAX(a,b) ((a)>(b) ? (a) : (b))

static int BMK_benchMem(const void* srcBuffer, size_t srcSize,
                        const char* displayName, int cLevel,
                        const size_t* fileSizes, U32 nbFiles,
                        const char* dictBuf, int dictSize)
{
    size_t const blockSize = (g_blockSize>=32 && !g_decodeOnly ? g_blockSize : srcSize) + (!srcSize) /* avoid div by 0 */ ;
    U32 const maxNbBlocks = (U32)((srcSize + (blockSize-1)) / blockSize) + nbFiles;
    blockParam_t* const blockTable = (blockParam_t*) malloc(maxNbBlocks * sizeof(blockParam_t));
    size_t const maxCompressedSize = (size_t)LZ4_compressBound((int)srcSize) + (maxNbBlocks * 1024);   /* add some room for safety */
    void* const compressedBuffer = malloc(maxCompressedSize);
    size_t const decMultiplier = g_decodeOnly ? 255 : 1;
    size_t const maxInSize = (size_t)LZ4_MAX_INPUT_SIZE / decMultiplier;
    size_t const maxDecSize = srcSize < maxInSize ? srcSize * decMultiplier : LZ4_MAX_INPUT_SIZE;
    void* const resultBuffer = malloc(maxDecSize);
    int benchError = 0;
    U32 nbBlocks;
    struct compressionParameters compP;

    /* checks */
    if (!compressedBuffer || !resultBuffer || !blockTable)
        END_PROCESS(31, "allocation error : not enough memory");

    if (strlen(displayName)>17) displayName += strlen(displayName)-17;   /* can only display 17 characters */

    /* init */
    LZ4_buildCompressionParameters(&compP, cLevel, dictBuf, dictSize);
    compP.initFunction(&compP);
    if (g_dctx==NULL) {
        LZ4F_createDecompressionContext(&g_dctx, LZ4F_VERSION);
        if (g_dctx==NULL)
            END_PROCESS(1, "allocation error - decompression state");
    }

    /* Init blockTable data */
    {   const char* srcPtr = (const char*)srcBuffer;
        char* cPtr = (char*)compressedBuffer;
        char* resPtr = (char*)resultBuffer;
        U32 fileNb;
        for (nbBlocks=0, fileNb=0; fileNb<nbFiles; fileNb++) {
            size_t remaining = fileSizes[fileNb];
            U32 const nbBlocksforThisFile = (U32)((remaining + (blockSize-1)) / blockSize);
            U32 const blockEnd = nbBlocks + nbBlocksforThisFile;
            for ( ; nbBlocks<blockEnd; nbBlocks++) {
                size_t const thisBlockSize = MIN(remaining, blockSize);
                size_t const resMaxSize = thisBlockSize * decMultiplier;
                size_t const resCapa = (thisBlockSize < maxInSize) ? resMaxSize : LZ4_MAX_INPUT_SIZE;
                blockTable[nbBlocks].srcPtr = srcPtr;
                blockTable[nbBlocks].cPtr = cPtr;
                blockTable[nbBlocks].resPtr = resPtr;
                blockTable[nbBlocks].srcSize = thisBlockSize;
                blockTable[nbBlocks].cRoom = (size_t)LZ4_compressBound((int)thisBlockSize);
                srcPtr += thisBlockSize;
                cPtr += blockTable[nbBlocks].cRoom;
                resPtr += resCapa;
                remaining -= thisBlockSize;
    }   }   }

    /* warming up memory */
    memset(compressedBuffer, ' ', maxCompressedSize);

    /* decode-only mode : copy input to @compressedBuffer */
    if (g_decodeOnly) {
        U32 blockNb;
        for (blockNb=0; blockNb < nbBlocks; blockNb++) {
            memcpy(blockTable[blockNb].cPtr, blockTable[blockNb].srcPtr, blockTable[blockNb].srcSize);
            blockTable[blockNb].cSize = blockTable[blockNb].srcSize;
    }   }

    /* Bench */
    {   U64 fastestC = (U64)(-1LL), fastestD = (U64)(-1LL);
        U64 const crcOrig = XXH64(srcBuffer, srcSize, 0);
        TIME_t coolTime = TIME_getTime();
        U64 const maxTime = (g_nbSeconds * TIMELOOP_NANOSEC) + 100;
        U32 nbCompressionLoops = (U32)((5 MB) / (srcSize+1)) + 1;  /* conservative initial compression speed estimate */
        U32 nbDecodeLoops = (U32)((200 MB) / (srcSize+1)) + 1;  /* conservative initial decode speed estimate */
        Duration_ns totalCTime=0, totalDTime=0;
        U32 cCompleted=(g_decodeOnly==1), dCompleted=0;
#       define NB_MARKS 4
        const char* const marks[NB_MARKS] = { " |", " /", " =",  "\\" };
        U32 markNb = 0;
        size_t cSize = srcSize;
        size_t totalRSize = srcSize;
        double ratio = 0.;

        DISPLAYLEVEL(2, "\r%79s\r", "");
        if (g_nbSeconds==0) { nbCompressionLoops = 1; nbDecodeLoops = 1; }
        while (!cCompleted || !dCompleted) {
            /* overheat protection */
            if (TIME_clockSpan_ns(coolTime) > ACTIVEPERIOD_NANOSEC) {
                DISPLAYLEVEL(2, "\rcooling down ...    \r");
                UTIL_sleep(COOLPERIOD_SEC);
                coolTime = TIME_getTime();
            }

            /* Compression */
            DISPLAYLEVEL(2, "%2s-%-17.17s :%10u ->\r", marks[markNb], displayName, (U32)totalRSize);
            if (!cCompleted) {
                memset(compressedBuffer, 0xE5, maxCompressedSize);  /* warm up and erase compressed buffer */
                { U32 blockNb; for (blockNb=0; blockNb<nbBlocks; blockNb++) blockTable[blockNb].cSize = 0; }
            }

            UTIL_sleepMilli(1);  /* give processor time to other processes */
            TIME_waitForNextTick();

            if (!cCompleted) {   /* still some time to do compression tests */
                TIME_t const timeStart = TIME_getTime();
                U32 nbLoops;
                for (nbLoops=0; nbLoops < nbCompressionLoops; nbLoops++) {
                    U32 blockNb;
                    compP.resetFunction(&compP);
                    for (blockNb=0; blockNb<nbBlocks; blockNb++) {
                        size_t const rSize = (size_t)compP.blockFunction(
                            &compP,
                            blockTable[blockNb].srcPtr, blockTable[blockNb].cPtr,
                            (int)blockTable[blockNb].srcSize, (int)blockTable[blockNb].cRoom);
                        if (LZ4_isError(rSize)) {
                            DISPLAY("LZ4 compression failed on block %u \n", blockNb);
                            benchError =1 ;
                        }
                        blockTable[blockNb].cSize = rSize;
                }   }
                {   Duration_ns const duration_ns = TIME_clockSpan_ns(timeStart);
                    if (duration_ns > 0) {
                        if (duration_ns < fastestC * nbCompressionLoops)
                            fastestC = duration_ns / nbCompressionLoops;
                        assert(fastestC > 0);
                        nbCompressionLoops = (U32)(TIMELOOP_NANOSEC / fastestC) + 1;  /* aim for ~1sec */
                    } else {
                        assert(nbCompressionLoops < 40000000);   /* avoid overflow */
                        nbCompressionLoops *= 100;
                    }
                    totalCTime += duration_ns;
                    cCompleted = totalCTime>maxTime;
                }

                cSize = 0;
                { U32 blockNb; for (blockNb=0; blockNb<nbBlocks; blockNb++) cSize += blockTable[blockNb].cSize; }
                cSize += !cSize;  /* avoid div by 0 */
                ratio = (double)totalRSize / (double)cSize;
                markNb = (markNb+1) % NB_MARKS;
                OUTLEVEL(2, "%2s-%-17.17s :%10u ->%10u (%5.3f),%6.1f MB/s\r",
                        marks[markNb], displayName,
                        (U32)totalRSize, (U32)cSize, ratio,
                        ((double)totalRSize / (double)fastestC) * 1000 );
                fflush(NULL);
            }
            (void)fastestD; (void)crcOrig;   /*  unused when decompression disabled */
#if 1
            /* Decompression */
            if (!dCompleted) memset(resultBuffer, 0xD6, srcSize);  /* warm result buffer */

            UTIL_sleepMilli(5); /* give processor time to other processes */
            TIME_waitForNextTick();

            if (!dCompleted) {
                const DecFunction_f decFunction = g_decodeOnly ?
                    LZ4F_decompress_binding : LZ4_decompress_safe_usingDict;
                const char* const decString = g_decodeOnly ?
                    "LZ4F_decompress" : "LZ4_decompress_safe_usingDict";
                TIME_t const timeStart = TIME_getTime();
                U32 nbLoops;

                for (nbLoops=0; nbLoops < nbDecodeLoops; nbLoops++) {
                    U32 blockNb;
                    for (blockNb=0; blockNb<nbBlocks; blockNb++) {
                        size_t const inMaxSize = (size_t)INT_MAX / decMultiplier;
                        size_t const resCapa = (blockTable[blockNb].srcSize < inMaxSize) ?
                                                blockTable[blockNb].srcSize * decMultiplier :
                                                INT_MAX;
                        int const regenSize = decFunction(
                            blockTable[blockNb].cPtr, blockTable[blockNb].resPtr,
                            (int)blockTable[blockNb].cSize, (int)resCapa,
                            dictBuf, dictSize);
                        if (regenSize < 0) {
                            DISPLAY("%s() failed on block %u of size %u \n",
                                decString, blockNb, (unsigned)blockTable[blockNb].srcSize);
                            if (g_decodeOnly)
                                DISPLAY("Is input using LZ4 Frame format ? \n");
                            benchError = 1;
                            break;
                        }
                        blockTable[blockNb].resSize = (size_t)regenSize;
                }   }
                {   Duration_ns const duration_ns = TIME_clockSpan_ns(timeStart);
                    if (duration_ns > 0) {
                        if (duration_ns < fastestD * nbDecodeLoops)
                            fastestD = duration_ns / nbDecodeLoops;
                        assert(fastestD > 0);
                        nbDecodeLoops = (U32)(TIMELOOP_NANOSEC / fastestD) + 1;  /* aim for ~1sec */
                    } else {
                        assert(nbDecodeLoops < 40000000);   /* avoid overflow */
                        nbDecodeLoops *= 100;
                    }
                    totalDTime += duration_ns;
                    dCompleted = totalDTime > (DECOMP_MULT*maxTime);
            }   }

            if (g_decodeOnly) {
                unsigned u;
                totalRSize = 0;
                for (u=0; u<nbBlocks; u++) totalRSize += blockTable[u].resSize;
            }
            markNb = (markNb+1) % NB_MARKS;
            ratio  = (double)totalRSize / (double)cSize;
            OUTLEVEL(2, "%2s-%-17.17s :%10u ->%10u (%5.3f),%6.1f MB/s,%7.1f MB/s\r",
                    marks[markNb], displayName,
                    (U32)totalRSize, (U32)cSize, ratio,
                    ((double)totalRSize / (double)fastestC) * 1000,
                    ((double)totalRSize / (double)fastestD) * 1000);
            fflush(NULL);

            /* CRC Checking (not possible in decode-only mode)*/
            if (!g_decodeOnly) {
                U64 const crcCheck = XXH64(resultBuffer, srcSize, 0);
                if (crcOrig!=crcCheck) {
                    size_t u;
                    DISPLAY("\n!!! WARNING !!! %17s : Invalid Checksum : %x != %x   \n", displayName, (unsigned)crcOrig, (unsigned)crcCheck);
                    benchError = 1;
                    for (u=0; u<srcSize; u++) {
                        if (((const BYTE*)srcBuffer)[u] != ((const BYTE*)resultBuffer)[u]) {
                            U32 segNb, bNb, pos;
                            size_t bacc = 0;
                            DISPLAY("Decoding error at pos %u ", (U32)u);
                            for (segNb = 0; segNb < nbBlocks; segNb++) {
                                if (bacc + blockTable[segNb].srcSize > u) break;
                                bacc += blockTable[segNb].srcSize;
                            }
                            pos = (U32)(u - bacc);
                            bNb = pos / (128 KB);
                            DISPLAY("(block %u, sub %u, pos %u) \n", segNb, bNb, pos);
                            break;
                        }
                        if (u==srcSize-1) {  /* should never happen */
                            DISPLAY("no difference detected\n");
                    }   }
                    break;
            }   }   /* CRC Checking */
#endif
        }   /* for (testNb = 1; testNb <= (g_nbSeconds + !g_nbSeconds); testNb++) */

        OUTLEVEL(2, "%2i#\n", cLevel);

        /* quiet mode */
        if (g_displayLevel == 1) {
            double const cSpeed = ((double)srcSize / (double)fastestC) * 1000;
            double const dSpeed = ((double)srcSize / (double)fastestD) * 1000;
            DISPLAYOUT("-%-3i%11i (%5.3f) %6.2f MB/s %6.1f MB/s  %s ", cLevel, (int)cSize, ratio, cSpeed, dSpeed, displayName);
            if (g_additionalParam)
                DISPLAYOUT("(param=%d)", g_additionalParam);
            DISPLAYOUT("\n");
        }
    }   /* Bench */

    /* clean up */
    compP.cleanupFunction(&compP);
    free(blockTable);
    free(compressedBuffer);
    free(resultBuffer);
    return benchError;
}


static size_t BMK_findMaxMem(U64 requiredMem)
{
    size_t step = 64 MB;
    BYTE* testmem=NULL;

    requiredMem = (((requiredMem >> 26) + 1) << 26);
    requiredMem += 2*step;
    if (requiredMem > maxMemory) requiredMem = maxMemory;

    while (!testmem) {
        if (requiredMem > step) requiredMem -= step;
        else requiredMem >>= 1;
        testmem = (BYTE*) malloc ((size_t)requiredMem);
    }
    free (testmem);

    /* keep some space available */
    if (requiredMem > step) requiredMem -= step;
    else requiredMem >>= 1;

    return (size_t)requiredMem;
}


static int BMK_benchCLevel(void* srcBuffer, size_t benchedSize,
                            const char* displayName, int cLevel, int cLevelLast,
                            const size_t* fileSizes, unsigned nbFiles,
                            const char* dictBuf, int dictSize)
{
    int l;
    int benchError = 0;
    const char* pch = strrchr(displayName, '\\'); /* Windows */
    if (!pch) pch = strrchr(displayName, '/'); /* Linux */
    if (pch) displayName = pch+1;

    SET_REALTIME_PRIORITY;

    if (g_displayLevel == 1 && !g_additionalParam)
        DISPLAY("bench %s %s: input %u bytes, %u seconds, %u KB blocks\n", LZ4_VERSION_STRING, LZ4_GIT_COMMIT_STRING, (U32)benchedSize, g_nbSeconds, (U32)(g_blockSize>>10));

    if (cLevelLast < cLevel) cLevelLast = cLevel;

    for (l=cLevel; l <= cLevelLast; l++) {
        benchError |= BMK_benchMem(
                            srcBuffer, benchedSize,
                            displayName, l,
                            fileSizes, nbFiles,
                            dictBuf, dictSize);
    }
    return benchError;
}


/*! BMK_loadFiles() :
    Loads `buffer` with content of files listed within `fileNamesTable`.
    At most, fills `buffer` entirely */
static void BMK_loadFiles(void* buffer, size_t bufferSize,
                          size_t* fileSizes,
                          const char** fileNamesTable, unsigned nbFiles)
{
    size_t pos = 0, totalSize = 0;
    unsigned n;
    for (n=0; n<nbFiles; n++) {
        FILE* f;
        U64 fileSize = UTIL_getFileSize(fileNamesTable[n]);
        if (UTIL_isDirectory(fileNamesTable[n])) {
            DISPLAYLEVEL(2, "Ignoring %s directory...       \n", fileNamesTable[n]);
            fileSizes[n] = 0;
            continue;
        }
        f = fopen(fileNamesTable[n], "rb");
        if (f==NULL) END_PROCESS(10, "impossible to open file %s", fileNamesTable[n]);
        DISPLAYUPDATE(2, "Loading %s...       \r", fileNamesTable[n]);
        if (fileSize > bufferSize-pos) { /* buffer too small - stop after this file */
            fileSize = bufferSize-pos;
            nbFiles=n;
        }
        { size_t const readSize = fread(((char*)buffer)+pos, 1, (size_t)fileSize, f);
          if (readSize != (size_t)fileSize) END_PROCESS(11, "could not read %s", fileNamesTable[n]);
          pos += readSize; }
        fileSizes[n] = (size_t)fileSize;
        totalSize += (size_t)fileSize;
        fclose(f);
    }

    if (totalSize == 0) END_PROCESS(12, "no data to bench");
}

static int BMK_benchFileTable(const char** fileNamesTable, unsigned nbFiles,
                              int cLevel, int cLevelLast,
                              const char* dictBuf, int dictSize)
{
    void* srcBuffer;
    size_t benchedSize;
    int benchError = 0;
    size_t* fileSizes = (size_t*)malloc(nbFiles * sizeof(size_t));
    U64 const totalSizeToLoad = UTIL_getTotalFileSize(fileNamesTable, nbFiles);
    char mfName[20] = {0};

    if (!fileSizes) END_PROCESS(12, "not enough memory for fileSizes");

    /* Memory allocation & restrictions */
    benchedSize = BMK_findMaxMem(totalSizeToLoad * 3) / 3;
    if (benchedSize==0) END_PROCESS(12, "not enough memory");
    if ((U64)benchedSize > totalSizeToLoad) benchedSize = (size_t)totalSizeToLoad;
    if (benchedSize > LZ4_MAX_INPUT_SIZE) {
        benchedSize = LZ4_MAX_INPUT_SIZE;
        DISPLAY("File(s) bigger than LZ4's max input size; testing %u MB only...\n", (U32)(benchedSize >> 20));
    } else {
        if (benchedSize < totalSizeToLoad)
            DISPLAY("Not enough memory; testing %u MB only...\n", (U32)(benchedSize >> 20));
    }
    srcBuffer = malloc(benchedSize + !benchedSize);   /* avoid alloc of zero */
    if (!srcBuffer) END_PROCESS(12, "not enough memory");

    /* Load input buffer */
    BMK_loadFiles(srcBuffer, benchedSize, fileSizes, fileNamesTable, nbFiles);

    /* Bench */
    snprintf (mfName, sizeof(mfName), " %u files", nbFiles);
    {   const char* displayName = (nbFiles > 1) ? mfName : fileNamesTable[0];
        benchError = BMK_benchCLevel(srcBuffer, benchedSize,
                        displayName, cLevel, cLevelLast,
                        fileSizes, nbFiles,
                        dictBuf, dictSize);
    }

    /* clean up */
    free(srcBuffer);
    free(fileSizes);
    return benchError;
}


static int BMK_syntheticTest(int cLevel, int cLevelLast,
                             const char* dictBuf, int dictSize)
{
    int benchError = 0;
    size_t const benchedSize = 10000000;
    void* const srcBuffer = malloc(benchedSize);

    /* Memory allocation */
    if (!srcBuffer) END_PROCESS(21, "not enough memory");

    /* Fill input buffer */
    LOREM_genBuffer(srcBuffer, benchedSize, 0);

    /* Bench */
    benchError = BMK_benchCLevel(srcBuffer, benchedSize,
                    "Lorem ipsum",
                    cLevel, cLevelLast,
                    &benchedSize,
                    1,
                    dictBuf, dictSize);

    /* clean up */
    free(srcBuffer);

    return benchError;
}


static int
BMK_benchFilesSeparately(const char** fileNamesTable, unsigned nbFiles,
                   int cLevel, int cLevelLast,
                   const char* dictBuf, int dictSize)
{
    int benchError = 0;
    unsigned fileNb;
    if (cLevel > LZ4HC_CLEVEL_MAX) cLevel = LZ4HC_CLEVEL_MAX;
    if (cLevelLast > LZ4HC_CLEVEL_MAX) cLevelLast = LZ4HC_CLEVEL_MAX;
    if (cLevelLast < cLevel) cLevelLast = cLevel;

    for (fileNb=0; fileNb<nbFiles; fileNb++)
        benchError |= BMK_benchFileTable(fileNamesTable+fileNb, 1, cLevel, cLevelLast, dictBuf, dictSize);

    return benchError;
}


int BMK_benchFiles(const char** fileNamesTable, unsigned nbFiles,
                   int cLevel, int cLevelLast,
                   const char* dictFileName)
{
    int benchError = 0;
    char* dictBuf = NULL;
    size_t dictSize = 0;

    if (cLevel > LZ4HC_CLEVEL_MAX) cLevel = LZ4HC_CLEVEL_MAX;
    if (g_decodeOnly) {
        DISPLAYLEVEL(2, "Benchmark Decompression of LZ4 Frame ");
        if (g_skipChecksums) {
            DISPLAYLEVEL(2, "_without_ checksum even when present \n");
        } else {
            DISPLAYLEVEL(2, "+ Checksum when present \n");
        }
        cLevelLast = cLevel;
    }
    if (cLevelLast > LZ4HC_CLEVEL_MAX) cLevelLast = LZ4HC_CLEVEL_MAX;
    if (cLevelLast < cLevel) cLevelLast = cLevel;
    if (cLevelLast > cLevel)
        DISPLAYLEVEL(2, "Benchmarking levels from %d to %d\n", cLevel, cLevelLast);

    if (dictFileName) {
        FILE* dictFile = NULL;
        U64 const dictFileSize = UTIL_getFileSize(dictFileName);
        if (!dictFileSize)
            END_PROCESS(25, "Dictionary error : could not stat dictionary file");
        if (g_decodeOnly)
            END_PROCESS(26, "Error : LZ4 Frame decoder mode not compatible with dictionary yet");

        dictFile = fopen(dictFileName, "rb");
        if (!dictFile)
            END_PROCESS(25, "Dictionary error : could not open dictionary file");

        if (dictFileSize > LZ4_MAX_DICT_SIZE) {
            dictSize = LZ4_MAX_DICT_SIZE;
            if (UTIL_fseek(dictFile, (long)(dictFileSize - dictSize), SEEK_SET))
                END_PROCESS(25, "Dictionary error : could not seek dictionary file");
        } else {
            dictSize = (size_t)dictFileSize;
        }

        dictBuf = (char*)malloc(dictSize);
        if (!dictBuf) END_PROCESS(25, "Allocation error : not enough memory");

        if (fread(dictBuf, 1, dictSize, dictFile) != dictSize)
            END_PROCESS(25, "Dictionary error : could not read dictionary file");

        fclose(dictFile);
    }

    if (nbFiles == 0) {
        benchError = BMK_syntheticTest(cLevel, cLevelLast, dictBuf, (int)dictSize);
    } else {
        if (g_benchSeparately)
            benchError = BMK_benchFilesSeparately(fileNamesTable, nbFiles, cLevel, cLevelLast, dictBuf, (int)dictSize);
        else
            benchError = BMK_benchFileTable(fileNamesTable, nbFiles, cLevel, cLevelLast, dictBuf, (int)dictSize);
    }

    free(dictBuf);
    return benchError;
}
