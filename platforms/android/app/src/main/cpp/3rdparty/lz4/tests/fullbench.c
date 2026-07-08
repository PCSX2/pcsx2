/*
    bench.c - Demo program to benchmark open-source compression algorithm
    Copyright (C) Yann Collet 2012-2020

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


#if defined(_MSC_VER) || defined(_WIN32)
   /* S_ISREG & gettimeofday() are not supported by MSVC */
#  define BMK_LEGACY_TIMER 1
#endif


/**************************************
*  Includes
**************************************/
#include "platform.h"    /* _CRT_SECURE_NO_WARNINGS, Large Files support */
#include "util.h"        /* U32, UTIL_getFileSize */
#include <stdlib.h>      /* malloc, free */
#include <stdio.h>       /* fprintf, fopen, ftello */
#include <sys/types.h>   /* stat64 */
#include <sys/stat.h>    /* stat64 */
#include <string.h>      /* strcmp */
#include <time.h>        /* clock_t, clock(), CLOCKS_PER_SEC */

#define LZ4_DISABLE_DEPRECATE_WARNINGS   /* LZ4_decompress_fast */
#include "lz4.h"
#include "lz4hc.h"
#include "lz4frame.h"

#include "xxhash.h"


/**************************************
*  Constants
**************************************/
#define PROGRAM_DESCRIPTION "LZ4 speed analyzer"
#define AUTHOR "Yann Collet"
#define WELCOME_MESSAGE "*** %s v%s %i-bits, by %s ***\n", PROGRAM_DESCRIPTION, LZ4_VERSION_STRING, (int)(sizeof(void*)*8), AUTHOR

#define NBLOOPS    6
#define TIMELOOP   (CLOCKS_PER_SEC * 19 / 10)

#define KB *(1 <<10)
#define MB *(1 <<20)
#define GB *(1U<<30)

#define KNUTH      2654435761U
#define MAX_MEM    (1920 MB)
#define DEFAULT_CHUNKSIZE   (4 MB)

#define ALL_COMPRESSORS 0
#define ALL_DECOMPRESSORS 0


/**************************************
*  Local structures
**************************************/
struct chunkParameters
{
    U32   id;
    char* origBuffer;
    char* compressedBuffer;
    int   origSize;
    int   compressedSize;
};


/**************************************
*  Macros
**************************************/
#define DISPLAY(...) fprintf(stderr, __VA_ARGS__)
#define PROGRESS(...) g_noPrompt ? 0 : DISPLAY(__VA_ARGS__)


/**************************************
*  Benchmark Parameters
**************************************/
static int g_chunkSize = DEFAULT_CHUNKSIZE;
static int g_nbIterations = NBLOOPS;
static int g_pause = 0;
static int g_compressionTest = 1;
static int g_compressionAlgo = ALL_COMPRESSORS;
static int g_decompressionTest = 1;
static int g_decompressionAlgo = ALL_DECOMPRESSORS;
static int g_noPrompt = 0;

static void BMK_setBlocksize(int bsize)
{
    g_chunkSize = bsize;
    DISPLAY("-Using Block Size of %i KB-\n", g_chunkSize>>10);
}

static void BMK_setNbIterations(int nbLoops)
{
    g_nbIterations = nbLoops;
    DISPLAY("- %i iterations -\n", g_nbIterations);
}

static void BMK_setPause(void)
{
    g_pause = 1;
}


/*********************************************************
*  Private functions
*********************************************************/
static clock_t BMK_GetClockSpan( clock_t clockStart )
{
    return clock() - clockStart;   /* works even if overflow; max span ~30 mn */
}


static size_t BMK_findMaxMem(U64 requiredMem)
{
    size_t step = 64 MB;
    BYTE* testmem = NULL;

    requiredMem = (((requiredMem >> 26) + 1) << 26);
    requiredMem += 2*step;
    if (requiredMem > MAX_MEM) requiredMem = MAX_MEM;

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


/*********************************************************
*  Memory management, to test LZ4_USER_MEMORY_FUNCTIONS
*********************************************************/
void* LZ4_malloc(size_t s) { return malloc(s); }
void* LZ4_calloc(size_t n, size_t s) { return calloc(n,s); }
void  LZ4_free(void* p) { free(p); }


/*********************************************************
*  Benchmark function
*********************************************************/
static LZ4_stream_t LZ4_stream;
static void local_LZ4_resetDictT(void)
{
    void* const r = LZ4_initStream(&LZ4_stream, sizeof(LZ4_stream));
    assert(r != NULL); (void)r;
}

static void local_LZ4_createStream(void)
{
    void* const r = LZ4_initStream(&LZ4_stream, sizeof(LZ4_stream));
    assert(r != NULL); (void)r;
}

static void* g_chunk0 = NULL;
static size_t g_chunk0Size = 0;
static void local_LZ4_saveDict_init(void)
{
    LZ4_loadDict(&LZ4_stream, (const char*)g_chunk0, (int)g_chunk0Size);
}

static int local_LZ4_saveDict(const char* in, char* out, int inSize)
{
    (void)in;
    return LZ4_saveDict(&LZ4_stream, out, inSize);
}

static int local_LZ4_compress_default_large(const char* in, char* out, int inSize)
{
    return LZ4_compress_default(in, out, inSize, LZ4_compressBound(inSize));
}

static int local_LZ4_compress_default_small(const char* in, char* out, int inSize)
{
    return LZ4_compress_default(in, out, inSize, LZ4_compressBound(inSize)-1);
}

static int local_LZ4_compress_destSize(const char* in, char* out, int inSize)
{
    return LZ4_compress_destSize(in, out, &inSize, LZ4_compressBound(inSize)-1);
}

static int local_LZ4_compress_fast0(const char* in, char* out, int inSize)
{
    return LZ4_compress_fast(in, out, inSize, LZ4_compressBound(inSize), 0);
}

static int local_LZ4_compress_fast1(const char* in, char* out, int inSize)
{
    return LZ4_compress_fast(in, out, inSize, LZ4_compressBound(inSize), 1);
}

static int local_LZ4_compress_fast2(const char* in, char* out, int inSize)
{
    return LZ4_compress_fast(in, out, inSize, LZ4_compressBound(inSize), 2);
}

static int local_LZ4_compress_fast17(const char* in, char* out, int inSize)
{
    return LZ4_compress_fast(in, out, inSize, LZ4_compressBound(inSize), 17);
}

static int local_LZ4_compress_fast_extState0(const char* in, char* out, int inSize)
{
    return LZ4_compress_fast_extState(&LZ4_stream, in, out, inSize, LZ4_compressBound(inSize), 0);
}

static int local_LZ4_compress_fast_continue0(const char* in, char* out, int inSize)
{
    return LZ4_compress_fast_continue(&LZ4_stream, in, out, inSize, LZ4_compressBound(inSize), 0);
}

#ifndef LZ4_DLL_IMPORT
#if defined (__cplusplus)
extern "C" {
#endif

/* declare hidden function */
extern int LZ4_compress_forceExtDict (LZ4_stream_t* LZ4_stream, const char* source, char* dest, int inputSize);

#if defined (__cplusplus)
}
#endif

static int local_LZ4_compress_forceDict(const char* in, char* out, int inSize)
{
    return LZ4_compress_forceExtDict(&LZ4_stream, in, out, inSize);
}
#endif


/* HC compression functions */
LZ4_streamHC_t LZ4_streamHC;
static void local_LZ4_resetStreamHC(void)
{
    LZ4_initStreamHC(&LZ4_streamHC, sizeof(LZ4_streamHC));
}

static void local_LZ4_saveDictHC_init(void)
{
    LZ4_loadDictHC(&LZ4_streamHC, (const char*)g_chunk0, (int)g_chunk0Size);
}

static int local_LZ4_saveDictHC(const char* in, char* out, int inSize)
{
    (void)in;
    return LZ4_saveDictHC(&LZ4_streamHC, out, inSize);
}

static int local_LZ4_compress_HC(const char* in, char* out, int inSize)
{
    return LZ4_compress_HC(in, out, inSize, LZ4_compressBound(inSize), 9);
}

static int local_LZ4_compress_HC_extStateHC(const char* in, char* out, int inSize)
{
    return LZ4_compress_HC_extStateHC(&LZ4_streamHC, in, out, inSize, LZ4_compressBound(inSize), 9);
}

static int local_LZ4_compress_HC_continue(const char* in, char* out, int inSize)
{
    return LZ4_compress_HC_continue(&LZ4_streamHC, in, out, inSize, LZ4_compressBound(inSize));
}


/* decompression functions */
static int local_LZ4_decompress_fast(const char* in, char* out, int inSize, int outSize)
{
    (void)inSize;
    LZ4_decompress_fast(in, out, outSize);
    return outSize;
}

static int local_LZ4_decompress_fast_usingDict_prefix(const char* in, char* out, int inSize, int outSize)
{
    (void)inSize;
    LZ4_decompress_fast_usingDict(in, out, outSize, out - 65536, 65536);
    return outSize;
}

static int local_LZ4_decompress_fast_usingExtDict(const char* in, char* out, int inSize, int outSize)
{
    (void)inSize;
    LZ4_decompress_fast_usingDict(in, out, outSize, out - 65536, 65535);
    return outSize;
}

static int local_LZ4_decompress_safe(const char* in, char* out, int inSize, int outSize)
{
    return LZ4_decompress_safe(in, out, inSize, outSize);
}

static int local_LZ4_decompress_safe_withPrefix64k(const char* in, char* out, int inSize, int outSize)
{
    LZ4_decompress_safe_withPrefix64k(in, out, inSize, outSize);
    return outSize;
}

static int local_LZ4_decompress_safe_usingDict(const char* in, char* out, int inSize, int outSize)
{
    LZ4_decompress_safe_usingDict(in, out, inSize, outSize, out - 65536, 65536);
    return outSize;
}

static int local_LZ4_decompress_safe_partial_usingDict(const char* in, char* out, int inSize, int outSize)
{
    int result = LZ4_decompress_safe_partial_usingDict(in, out, inSize, outSize - 5, outSize, out - 65536, 65536);
    if (result < 0) return result;
    return outSize;
}

#ifndef LZ4_DLL_IMPORT
#if defined (__cplusplus)
extern "C" {
#endif

extern int LZ4_decompress_safe_forceExtDict(const char* in, char* out, int inSize, int outSize, const void* dict, size_t dictSize);

#if defined (__cplusplus)
}
#endif

static int local_LZ4_decompress_safe_forceExtDict(const char* in, char* out, int inSize, int outSize)
{
    LZ4_decompress_safe_forceExtDict(in, out, inSize, outSize, out - 65536, 65536);
    return outSize;
}
#endif

#ifndef LZ4_DLL_IMPORT
#if defined (__cplusplus)
extern "C" {
#endif

extern int LZ4_decompress_safe_partial_forceExtDict(const char* in, char* out, int inSize, int targetOutputSize, int dstCapacity, const void* dict, size_t dictSize);

#if defined (__cplusplus)
}
#endif

static int local_LZ4_decompress_safe_partial_forceExtDict(const char* in, char* out, int inSize, int outSize)
{
    int result = LZ4_decompress_safe_partial_forceExtDict(in, out, inSize, outSize - 5, outSize, out - 65536, 65536);
    if (result < 0) return result;
    return outSize;
}
#endif

static int local_LZ4_decompress_safe_partial(const char* in, char* out, int inSize, int outSize)
{
    int result = LZ4_decompress_safe_partial(in, out, inSize, outSize - 5, outSize);
    if (result < 0) return result;
    return outSize;
}


/* frame functions */
static int local_LZ4F_compressFrame(const char* in, char* out, int inSize)
{
    assert(inSize >= 0);
    return (int)LZ4F_compressFrame(out, LZ4F_compressFrameBound((size_t)inSize, NULL), in, (size_t)inSize, NULL);
}

LZ4F_cctx* g_cctx = NULL;
static int local_LZ4F_compress(const char* in, char* out, int inSize)
{
    /* output buffer size is assumed */
    size_t const outSize = LZ4F_compressFrameBound((size_t)inSize, NULL);
    size_t cSize = 0;
    assert(inSize >= 0);
    if (g_cctx == NULL) {
        /* create and initialize LZ4F compression context the first time */
        LZ4F_createCompressionContext(&g_cctx, LZ4F_VERSION);
        assert(g_cctx != NULL);
    } /* re-use existing compression context otherwise */
    {   size_t const cbSize = LZ4F_compressBegin(g_cctx, out, outSize, NULL);
        assert(!LZ4F_isError(cbSize));
        cSize += cbSize;
    }
    {   size_t const cuSize = LZ4F_compressUpdate(g_cctx, out+cSize, outSize-cSize, in, (size_t)inSize, NULL);
        assert(!LZ4F_isError(cuSize));
        cSize += cuSize;
    }
    {   size_t const ceSize = LZ4F_compressEnd(g_cctx, out+cSize, outSize-cSize, NULL);
        assert(!LZ4F_isError(ceSize));
        cSize += ceSize;
    }
    return (int)cSize;
}

static LZ4F_decompressionContext_t g_dCtx;

static int local_LZ4F_decompress(const char* in, char* out, int inSize, int outSize)
{
    size_t srcSize = (size_t)inSize;
    size_t dstSize = (size_t)outSize;
    size_t result;
    assert(inSize >= 0);
    assert(outSize >= 0);
    result = LZ4F_decompress(g_dCtx, out, &dstSize, in, &srcSize, NULL);
    if (result!=0) { DISPLAY("Error decompressing frame : unfinished frame \n"); exit(8); }
    if (srcSize != (size_t)inSize) { DISPLAY("Error decompressing frame : read size incorrect \n"); exit(9); }
    return (int)dstSize;
}

static int local_LZ4F_decompress_followHint(const char* src, char* dst, int srcSize, int dstSize)
{
    size_t totalInSize = (size_t)srcSize;
    size_t maxOutSize = (size_t)dstSize;

    size_t inPos = 0;
    size_t inSize = 0;
    size_t outPos = 0;
    size_t outRemaining = maxOutSize - outPos;

    for (;;) {
        size_t const sizeHint =
            LZ4F_decompress(g_dCtx, dst+outPos, &outRemaining, src+inPos, &inSize, NULL);
        assert(!LZ4F_isError(sizeHint));

        inPos += inSize;
        inSize = sizeHint;

        outPos += outRemaining;
        outRemaining = maxOutSize - outPos;

        if (!sizeHint) break;
    }

    /* frame completed */
    if (inPos != totalInSize) {
        DISPLAY("Error decompressing frame : must read (%u) full frame (%u) \n",
                (unsigned)inPos, (unsigned)totalInSize);
        exit(10);
    }
    return (int)outPos;

}

/* always provide input by block of 64 KB */
static int local_LZ4F_decompress_noHint(const char* src, char* dst, int srcSize, int dstSize)
{
    size_t totalInSize = (size_t)srcSize;
    size_t maxOutSize = (size_t)dstSize;

    size_t inPos = 0;
    size_t inSize = 64 KB;
    size_t outPos = 0;
    size_t outRemaining = maxOutSize - outPos;

    for (;;) {
        size_t const sizeHint = LZ4F_decompress(g_dCtx, dst+outPos, &outRemaining, src+inPos, &inSize, NULL);
        assert(!LZ4F_isError(sizeHint));

        inPos += inSize;
        inSize = (inPos + 64 KB <= totalInSize) ? 64 KB : totalInSize - inPos;

        outPos += outRemaining;
        outRemaining = maxOutSize - outPos;

        if (!sizeHint) break;
    }

    /* frame completed */
    if (inPos != totalInSize) {
        DISPLAY("Error decompressing frame : must read (%u) full frame (%u) \n",
                (unsigned)inPos, (unsigned)totalInSize);
        exit(10);
    }
    return (int)outPos;

}

typedef struct {
    const char* name;
    int (*compressionF)(const char*, char*, int);
    void (*initFunction)(void);
    int singleChunk;
} CompressionDesc;

static const CompressionDesc compDescArray[] = {
    { NULL, NULL, NULL, 0 },
    { "LZ4_compress_default", local_LZ4_compress_default_large, NULL, 0 },
    { "LZ4_compress_default(small dst)", local_LZ4_compress_default_small, NULL, 0 },
    { "LZ4_compress_destSize", local_LZ4_compress_destSize, NULL, 0 },
    { "LZ4_compress_fast(0)", local_LZ4_compress_fast0, NULL, 0 },
    { "LZ4_compress_fast(1)", local_LZ4_compress_fast1, NULL, 0 },
    { "LZ4_compress_fast(2)", local_LZ4_compress_fast2, NULL, 0 },
    { "LZ4_compress_fast(17)", local_LZ4_compress_fast17, NULL, 0 },
    { "LZ4_compress_fast_extState(0)", local_LZ4_compress_fast_extState0, NULL, 0 },
    { "LZ4_compress_fast_continue(0)", local_LZ4_compress_fast_continue0, local_LZ4_createStream, 0 },
    { "LZ4_compress_HC", local_LZ4_compress_HC, NULL, 0 },
    { "LZ4_compress_HC_extStateHC", local_LZ4_compress_HC_extStateHC, NULL, 0 },
    { "LZ4_compress_HC_continue", local_LZ4_compress_HC_continue, local_LZ4_resetStreamHC, 0 },
#ifndef LZ4_DLL_IMPORT
    { "LZ4_compress_forceDict", local_LZ4_compress_forceDict, local_LZ4_resetDictT, 0 },
#endif
    { "LZ4F_compressFrame", local_LZ4F_compressFrame, NULL, 1 },
    { "LZ4F_compressUpdate", local_LZ4F_compress, NULL, 1 },
    { "LZ4_saveDict", local_LZ4_saveDict, local_LZ4_saveDict_init, 0 },
    { "LZ4_saveDictHC", local_LZ4_saveDictHC, local_LZ4_saveDictHC_init, 0 },

};

typedef struct {
    const char* name;
    int (*decompressionF)(const char*, char*, int, int);
    int checkResult;
    int frameFormat;
} DecompressionDesc;

static const DecompressionDesc decDescArray[] = {
    { NULL, NULL, 0, 0 },
    { "LZ4_decompress_fast", local_LZ4_decompress_fast, 1, 0 },
    { "LZ4_decompress_fast_usingDict(prefix)", local_LZ4_decompress_fast_usingDict_prefix, 1, 0 },
    { "LZ4_decompress_fast_using(Ext)Dict", local_LZ4_decompress_fast_usingExtDict, 1, 0 },
    { "LZ4_decompress_safe", local_LZ4_decompress_safe, 1, 0 },
    { "LZ4_decompress_safe_withPrefix64k", local_LZ4_decompress_safe_withPrefix64k, 1, 0 },
    { "LZ4_decompress_safe_usingDict", local_LZ4_decompress_safe_usingDict, 1, 0 },
    { "LZ4_decompress_safe_partial", local_LZ4_decompress_safe_partial, 0, 0 },
    { "LZ4_decompress_safe_partial_usingDict", local_LZ4_decompress_safe_partial_usingDict, 0, 0 },
#ifndef LZ4_DLL_IMPORT
    { "LZ4_decompress_safe_partial_forceExtDict", local_LZ4_decompress_safe_partial_forceExtDict, 0, 0 },
    { "LZ4_decompress_safe_forceExtDict", local_LZ4_decompress_safe_forceExtDict, 1, 0 },
#endif
    { "LZ4F_decompress", local_LZ4F_decompress, 1, 1 },
    { "LZ4F_decompress_followHint", local_LZ4F_decompress_followHint, 1, 1 },
    { "LZ4F_decompress_noHint", local_LZ4F_decompress_noHint, 1, 1 },
};

#define ARRAY_SIZE(a) (sizeof(a) / (sizeof((a)[0])))

#define NB_COMPRESSION_ALGORITHMS ARRAY_SIZE(compDescArray)
#define NB_DECOMPRESSION_ALGORITHMS ARRAY_SIZE(decDescArray)

int fullSpeedBench(const char** fileNamesTable, int nbFiles)
{
    int fileIdx=0;
    clock_t loopDuration = TIMELOOP;

    if (g_nbIterations==0) {
        loopDuration = CLOCKS_PER_SEC / 50 + 1;
        g_nbIterations = 1;
    }

    /* Init */
    { size_t const errorCode = LZ4F_createDecompressionContext(&g_dCtx, LZ4F_VERSION);
      if (LZ4F_isError(errorCode)) { DISPLAY("dctx allocation issue \n"); return 10; } }

    /* Loop for each fileName */
    while (fileIdx<nbFiles) {
      char* orig_buff = NULL;
      struct chunkParameters* chunkP = NULL;
      char* compressed_buff=NULL;
      const char* const inFileName = fileNamesTable[fileIdx++];
      FILE* const inFile = fopen( inFileName, "rb" );
      U64 const inFileSize = UTIL_getFileSize(inFileName);
      size_t benchedSize = BMK_findMaxMem(inFileSize*2) / 2;   /* because 2 buffers */
      int nbChunks;
      int maxCompressedChunkSize;
      size_t readSize;
      int compressedBuffSize;
      U32 crcOriginal;

      /* Check infile pre-requisites */
      if (inFile==NULL) { DISPLAY("Pb opening %s \n", inFileName); return 11; }
      if (inFileSize==0) { DISPLAY("file is empty \n"); fclose(inFile); return 11; }
      if (benchedSize==0) { DISPLAY("not enough memory \n"); fclose(inFile); return 11; }

      /* Memory size adjustments */
      if ((U64)benchedSize > inFileSize) benchedSize = (size_t)inFileSize;
      if (benchedSize < inFileSize) {
          DISPLAY("Not enough memory for '%s' full size; testing %i MB only... \n",
                inFileName, (int)(benchedSize>>20));
      }
      if (benchedSize < 8 || g_chunkSize < 8) {
        DISPLAY(" cannot bench using less then 8 bytes \n");
        return 1;
      }

      /* Allocation */
      nbChunks = (int) ((benchedSize + (size_t)g_chunkSize - 1) / (size_t)g_chunkSize);
      chunkP = (struct chunkParameters*) malloc((size_t)nbChunks * sizeof(chunkP[0]));
      orig_buff = (char*) malloc(benchedSize);
      maxCompressedChunkSize = LZ4_compressBound(g_chunkSize);
      compressedBuffSize = nbChunks * maxCompressedChunkSize;
      compressed_buff = (char*)malloc((size_t)compressedBuffSize);
      if(!chunkP || !orig_buff || !compressed_buff) {
          DISPLAY("\nError: not enough memory! \n");
          fclose(inFile);
          free(orig_buff);
          free(compressed_buff);
          free(chunkP);
          return(12);
      }

      /* Fill in src buffer */
      DISPLAY("Loading %s...       \r", inFileName);
      readSize = fread(orig_buff, 1, benchedSize, inFile);
      fclose(inFile);

      if (readSize != benchedSize) {
        DISPLAY("\nError: problem reading file '%s' !!    \n", inFileName);
        free(orig_buff);
        free(compressed_buff);
        free(chunkP);
        return 13;
      }

      /* Calculating input Checksum */
      crcOriginal = XXH32(orig_buff, benchedSize,0);

      /* Bench */
      { int loopNb, nb_loops, chunkNb, cAlgNb, dAlgNb;
        size_t cSize=0;
        double ratio=0.;

        DISPLAY("\r%79s\r", "");
        DISPLAY(" %s : \n", inFileName);

        /* Bench Compression Algorithms */
        for (cAlgNb=0; (cAlgNb <= (int)NB_COMPRESSION_ALGORITHMS) && (g_compressionTest); cAlgNb++) {
            const char* compressorName;
            int (*compressionFunction)(const char*, char*, int);
            void (*initFunction)(void) = NULL;
            double bestTime = 100000000.;

            /* filter compressionAlgo only */
            if ((g_compressionAlgo != ALL_COMPRESSORS) && (g_compressionAlgo != cAlgNb)) continue;

            /* Init data chunks */
            {   int i;
                size_t remaining = benchedSize;
                char* in = orig_buff;
                char* out = compressed_buff;
                assert(nbChunks >= 1);
                for (i=0; i<nbChunks; i++) {
                    chunkP[i].id = (U32)i;
                    chunkP[i].origBuffer = in; in += g_chunkSize;
                    assert(g_chunkSize > 0);
                    if (remaining > (size_t)g_chunkSize) {
                        chunkP[i].origSize = g_chunkSize;
                        remaining -= (size_t)g_chunkSize;
                    } else {
                        chunkP[i].origSize = (int)remaining;
                        remaining = 0;
                    }
                    chunkP[i].compressedBuffer = out; out += maxCompressedChunkSize;
                    chunkP[i].compressedSize = 0;
                }
            }
            g_chunk0 = chunkP[0].origBuffer;
            g_chunk0Size = (size_t)chunkP[0].origSize;

            if (cAlgNb==0) {
                DISPLAY("Compression functions : \n");
                continue;
            }
            if (cAlgNb >= (int)NB_COMPRESSION_ALGORITHMS) {
                continue;
            }
            compressorName = compDescArray[cAlgNb].name;
            compressionFunction = compDescArray[cAlgNb].compressionF;
            initFunction = compDescArray[cAlgNb].initFunction;
            if (compDescArray[cAlgNb].singleChunk) {
                nbChunks=1;
                chunkP[0].origSize = (int)benchedSize;
            }
            if (compressorName==NULL || compressionFunction==NULL) {
                continue;
            }

            for (loopNb = 1; loopNb <= g_nbIterations; loopNb++) {
                double averageTime;
                clock_t clockTime;

                PROGRESS("%2i-%-34.34s :%10i ->\r", loopNb, compressorName, (int)benchedSize);
                { size_t i; for (i=0; i<benchedSize; i++) compressed_buff[i]=(char)i; }     /* warming up memory */

                nb_loops = 0;
                clockTime = clock();
                while(clock() == clockTime);
                clockTime = clock();
                while(BMK_GetClockSpan(clockTime) < loopDuration) {
                    if (initFunction!=NULL) initFunction();
                    for (chunkNb=0; chunkNb<nbChunks; chunkNb++) {
                        chunkP[chunkNb].compressedSize = compressionFunction(chunkP[chunkNb].origBuffer, chunkP[chunkNb].compressedBuffer, chunkP[chunkNb].origSize);
                        if (chunkP[chunkNb].compressedSize==0) {
                            DISPLAY("ERROR ! %s() = 0 !! \n", compressorName);
                            exit(1);
                    }   }
                    nb_loops++;
                }
                clockTime = BMK_GetClockSpan(clockTime);

                nb_loops += !nb_loops;   /* avoid division by zero */
                averageTime = ((double)clockTime) / nb_loops / CLOCKS_PER_SEC;
                if (averageTime < bestTime) bestTime = averageTime;
                cSize=0; for (chunkNb=0; chunkNb<nbChunks; chunkNb++) cSize += (size_t)chunkP[chunkNb].compressedSize;
                ratio = (double)cSize/(double)benchedSize*100.;
                PROGRESS("%2i-%-34.34s :%10i ->%9i (%5.2f%%),%7.1f MB/s\r", loopNb, compressorName, (int)benchedSize, (int)cSize, ratio, (double)benchedSize / bestTime / 1000000);
            }

            if (ratio<100.)
                DISPLAY("%2i-%-34.34s :%10i ->%9i (%5.2f%%),%7.1f MB/s\n", cAlgNb, compressorName, (int)benchedSize, (int)cSize, ratio, (double)benchedSize / bestTime / 1000000);
            else
                DISPLAY("%2i-%-34.34s :%10i ->%9i (%5.1f%%),%7.1f MB/s\n", cAlgNb, compressorName, (int)benchedSize, (int)cSize, ratio, (double)benchedSize / bestTime / 100000);
        }

        /* Prepare layout for decompression */
        /* Init data chunks */
        { int i;
          size_t remaining = benchedSize;
          char* in = orig_buff;
          char* out = compressed_buff;

          nbChunks = (int) (((int)benchedSize + (g_chunkSize-1))/ g_chunkSize);
          for (i=0; i<nbChunks; i++) {
              chunkP[i].id = (U32)i;
              chunkP[i].origBuffer = in; in += g_chunkSize;
              if ((int)remaining > g_chunkSize) {
                  chunkP[i].origSize = g_chunkSize;
                  remaining -= (size_t)g_chunkSize;
              } else {
                  chunkP[i].origSize = (int)remaining;
                  remaining = 0;
              }
              chunkP[i].compressedBuffer = out; out += maxCompressedChunkSize;
              chunkP[i].compressedSize = 0;
          }
        }
        for (chunkNb=0; chunkNb<nbChunks; chunkNb++) {
            chunkP[chunkNb].compressedSize = LZ4_compress_default(chunkP[chunkNb].origBuffer, chunkP[chunkNb].compressedBuffer, chunkP[chunkNb].origSize, maxCompressedChunkSize);
            if (chunkP[chunkNb].compressedSize==0) {
                DISPLAY("ERROR ! %s() = 0 !! \n", "LZ4_compress");
                exit(1);
        }   }

        /* Decompression Algorithms */
        for (dAlgNb=0; (dAlgNb <= (int)NB_DECOMPRESSION_ALGORITHMS) && g_decompressionTest; dAlgNb++) {
            const char* dName = NULL;
            int (*decompressionFunction)(const char*, char*, int, int) = NULL;
            double bestTime = 100000000.;
            int checkResult = 1;

            if ((g_decompressionAlgo != ALL_DECOMPRESSORS) && (g_decompressionAlgo != dAlgNb)) continue;

            if (dAlgNb==0) { /* just for display */
                DISPLAY("Decompression functions : \n");
                continue;
            }
            if (dAlgNb >= (int)NB_DECOMPRESSION_ALGORITHMS)
                continue;
            dName = decDescArray[dAlgNb].name;
            decompressionFunction = decDescArray[dAlgNb].decompressionF;
            if (dName==NULL || decompressionFunction == NULL)
                continue;
            checkResult = decDescArray[dAlgNb].checkResult;
            if (decDescArray[dAlgNb].frameFormat) {
                /* prepare compressed data using LZ4F frame format */
                size_t const fcsize = LZ4F_compressFrame(compressed_buff, (size_t)compressedBuffSize, orig_buff, benchedSize, NULL);
                assert(!LZ4F_isError(fcsize));
                chunkP[0].origSize = (int)benchedSize;
                chunkP[0].compressedSize = (int)fcsize;
                nbChunks = 1;
            }

            { size_t i; for (i=0; i<benchedSize; i++) orig_buff[i]=0; }     /* zeroing source area, for CRC checking */

            for (loopNb = 1; loopNb <= g_nbIterations; loopNb++) {
                double averageTime;
                clock_t clockTime;
                U32 crcDecoded;

                PROGRESS("%2i-%-34.34s :%10i ->\r", loopNb, dName, (int)benchedSize);

                nb_loops = 0;
                clockTime = clock();
                while(clock() == clockTime);
                clockTime = clock();
                while(BMK_GetClockSpan(clockTime) < loopDuration) {
                    for (chunkNb=0; chunkNb<nbChunks; chunkNb++) {
                        int const decodedSize = decompressionFunction(chunkP[chunkNb].compressedBuffer, chunkP[chunkNb].origBuffer,
                                                                      chunkP[chunkNb].compressedSize, chunkP[chunkNb].origSize);
                        if (chunkP[chunkNb].origSize != decodedSize) {
                            DISPLAY("ERROR ! %s() == %i != %i !! \n",
                                    dName, decodedSize, chunkP[chunkNb].origSize);
                            exit(1);
                    }   }
                    nb_loops++;
                }
                clockTime = BMK_GetClockSpan(clockTime);

                nb_loops += !nb_loops;   /* Avoid division by zero */
                averageTime = (double)clockTime / nb_loops / CLOCKS_PER_SEC;
                if (averageTime < bestTime) bestTime = averageTime;

                PROGRESS("%2i-%-34.34s :%10i -> %7.1f MB/s\r", loopNb, dName, (int)benchedSize, (double)benchedSize / bestTime / 1000000);

                /* CRC Checking */
                crcDecoded = XXH32(orig_buff, benchedSize, 0);
                if (checkResult && (crcOriginal!=crcDecoded)) {
                    DISPLAY("\n!!! WARNING !!! %14s : Invalid Checksum : %x != %x\n",
                            inFileName, (unsigned)crcOriginal, (unsigned)crcDecoded);
                    exit(1);
            }   }

            DISPLAY("%2i-%-34.34s :%10i -> %7.1f MB/s\n", dAlgNb, dName, (int)benchedSize, (double)benchedSize / bestTime / 1000000);
        }
      }
      free(orig_buff);
      free(compressed_buff);
      free(chunkP);
    }

    LZ4F_freeDecompressionContext(g_dCtx);
    if (g_pause) { printf("press enter...\n"); (void)getchar(); }

    return 0;
}

static int list(void)
{
    size_t n;
    DISPLAY("Compression Algorithm ids: \n");
    for (n=0; n<NB_COMPRESSION_ALGORITHMS; n++) {
        if (compDescArray[n].name) {
            DISPLAY("%2u: %s \n", (unsigned)n, compDescArray[n].name);
        }
    }
    DISPLAY("Decompression Algorithm ids: \n");
    for (n=0; n<NB_DECOMPRESSION_ALGORITHMS; n++) {
        if (decDescArray[n].name) {
            DISPLAY("%2u: %s \n", (unsigned)n, decDescArray[n].name);
        }
    }
    return 0;
}

static int usage(const char* exename)
{
    DISPLAY( "Usage :\n");
    DISPLAY( "      %s [arg] file1 file2 ... fileX\n", exename);
    DISPLAY( "Arguments :\n");
    DISPLAY( " -c     : compression tests only\n");
    DISPLAY( " -d     : decompression tests only\n");
    DISPLAY( " -H/-h  : Help (this text + advanced options)\n");
    return 0;
}

static int usage_advanced(void)
{
    DISPLAY( "\nAdvanced options :\n");
    DISPLAY( " -c#    : test only compression function # [1-%i]\n", (int)NB_COMPRESSION_ALGORITHMS);
    DISPLAY( " -d#    : test only decompression function # [1-%i]\n", (int)NB_DECOMPRESSION_ALGORITHMS);
    DISPLAY( " -i#    : iteration loops [1-9](default : %i)\n", NBLOOPS);
    DISPLAY( " -B#    : Block size [4-7](default : 7)\n");
    return list();
}

static int badusage(const char* exename)
{
    DISPLAY("Wrong parameters\n");
    usage(exename);
    return 0;
}

int main(int argc, const char** argv)
{
    int i,
        filenamesStart=2;
    const char* exename = argv[0];
    const char* input_filename=0;

    // Welcome message
    DISPLAY(WELCOME_MESSAGE);

    if (argc<2) { badusage(exename); return 1; }

    for(i=1; i<argc; i++) {
        const char* argument = argv[i];

        if(!argument) continue;   // Protection if argument empty
        if (!strcmp(argument, "--no-prompt")) {
            g_noPrompt = 1;
            continue;
        }

        // Decode command (note : aggregated commands are allowed)
        if (argument[0]=='-') {
            while (argument[1]!=0) {
                argument ++;

                switch(argument[0])
                {
                    // Select compression algorithm only
                case 'c':
                    g_decompressionTest = 0;
                    while ((argument[1]>= '0') && (argument[1]<= '9')) {
                        g_compressionAlgo *= 10;
                        g_compressionAlgo += argument[1] - '0';
                        argument++;
                    }
                    break;

                    // Select decompression algorithm only
                case 'd':
                    g_compressionTest = 0;
                    while ((argument[1]>= '0') && (argument[1]<= '9')) {
                        g_decompressionAlgo *= 10;
                        g_decompressionAlgo += argument[1] - '0';
                        argument++;
                    }
                    break;

                    // Display help on usage
                case 'h' :
                case 'H': usage(exename); return usage_advanced();
                case 'l': return list();

                    // Modify Block Properties
                case 'B':
                    while (argument[1]!=0)
                    switch(argument[1])
                    {
                    case '4':
                    case '5':
                    case '6':
                    case '7':
                    {   int B = argument[1] - '0';
                        int S = 1 << (8 + 2*B);
                        BMK_setBlocksize(S);
                        argument++;
                        break;
                    }
                    case 'D': argument++; break;
                    default : goto _exit_blockProperties;
                    }
_exit_blockProperties:
                    break;

                    // Modify Nb Iterations
                case 'i':
                    if ((argument[1] >='0') && (argument[1] <='9')) {
                        int iters = argument[1] - '0';
                        BMK_setNbIterations(iters);
                        argument++;
                    }
                    break;

                    // Pause at the end (hidden option)
                case 'p': BMK_setPause(); break;

                    // Unknown command
                default : badusage(exename); return 1;
                }
            }
            continue;
        }

        // first provided filename is input
        if (!input_filename) { input_filename=argument; filenamesStart=i; continue; }

    }

    // No input filename ==> Error
    if(!input_filename) { badusage(exename); return 1; }

    return fullSpeedBench(argv+filenamesStart, argc-filenamesStart);

}
