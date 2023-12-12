/*
   LZ4 - Fast LZ compression algorithm
   Copyright (C) 2011-2014, Yann Collet.
   BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

       * Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
       * Redistributions in binary form must reproduce the above
   copyright notice, this list of conditions and the following disclaimer
   in the documentation and/or other materials provided with the
   distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   You can contact the author at :
   - LZ4 source repository : http://code.google.com/p/lz4/
   - LZ4 public forum : https://groups.google.com/forum/#!forum/lz4c
*/

/**************************************
   Tuning parameters
**************************************/
/*
 * MEMORY_USAGE :
 * Memory usage formula : N->2^N Bytes (examples : 10 -> 1KB; 12 -> 4KB ; 16 -> 64KB; 20 -> 1MB; etc.)
 * Increasing memory usage improves compression ratio
 * Reduced memory usage can improve speed, due to cache effect
 * Default value is 14, for 16KB, which nicely fits into Intel x86 L1 cache
 */
#define MEMORY_USAGE 14

/*
 * HEAPMODE :
 * Select how default compression functions will allocate memory for their hash table,
 * in memory stack (0:default, fastest), or in memory heap (1:requires memory allocation (malloc)).
 */
#define HEAPMODE 0


/**************************************
   CPU Feature Detection
**************************************/
/* 32 or 64 bits ? */
#if (defined(__x86_64__) || defined(_M_X64) || defined(_WIN64) || defined(__powerpc64__) || defined(__ppc64__) || defined(__PPC64__) || defined(__64BIT__) || defined(_LP64) || defined(__LP64__) || defined(__ia64) || defined(__itanium__) || defined(_M_IA64)) /* Detects 64 bits mode */
#define LZ4_ARCH64 1
#else
#define LZ4_ARCH64 0
#endif

/*
 * Little Endian or Big Endian ?
 * Overwrite the #define below if you know your architecture endianess
 */
#if defined(__GLIBC__)
#include <endian.h>
#if (__BYTE_ORDER == __BIG_ENDIAN)
#define LZ4_BIG_ENDIAN 1
#endif
#elif (defined(__BIG_ENDIAN__) || defined(__BIG_ENDIAN) || defined(_BIG_ENDIAN)) && !(defined(__LITTLE_ENDIAN__) || defined(__LITTLE_ENDIAN) || defined(_LITTLE_ENDIAN))
#define LZ4_BIG_ENDIAN 1
#elif defined(__sparc) || defined(__sparc__) || defined(__powerpc__) || defined(__ppc__) || defined(__PPC__) || defined(__hpux) || defined(__hppa) || defined(_MIPSEB) || defined(__s390__)
#define LZ4_BIG_ENDIAN 1
#else
/* Little Endian assumed. PDP Endian and other very rare endian format are unsupported. */
#endif

/*
 * Unaligned memory access is automatically enabled for "common" CPU, such as x86.
 * For others CPU, such as ARM, the compiler may be more cautious, inserting unnecessary extra code to ensure aligned access property
 * If you know your target CPU supports unaligned memory access, you want to force this option manually to improve performance
 */
#if defined(__ARM_FEATURE_UNALIGNED)
#define LZ4_FORCE_UNALIGNED_ACCESS 1
#endif

/* Define this parameter if your target system or compiler does not support hardware bit count */
#if defined(_MSC_VER) && defined(_WIN32_WCE) /* Visual Studio for Windows CE does not support Hardware bit count */
#define LZ4_FORCE_SW_BITCOUNT
#endif

/*
 * BIG_ENDIAN_NATIVE_BUT_INCOMPATIBLE :
 * This option may provide a small boost to performance for some big endian cpu, although probably modest.
 * You may set this option to 1 if data will remain within closed environment.
 * This option is useless on Little_Endian CPU (such as x86)
 */

/* #define BIG_ENDIAN_NATIVE_BUT_INCOMPATIBLE 1 */


/**************************************
 Compiler Options
**************************************/
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) /* C99 */
/* "restrict" is a known keyword */
#else
#define restrict /* Disable restrict */
#endif

#ifdef _MSC_VER /* Visual Studio */
#define FORCE_INLINE static __forceinline
#include <intrin.h>                  /* For Visual 2005 */
#if LZ4_ARCH64                       /* 64-bits */
#pragma intrinsic(_BitScanForward64) /* For Visual 2005 */
#pragma intrinsic(_BitScanReverse64) /* For Visual 2005 */
#else                                /* 32-bits */
#pragma intrinsic(_BitScanForward)   /* For Visual 2005 */
#pragma intrinsic(_BitScanReverse)   /* For Visual 2005 */
#endif
#pragma warning(disable : 4127) /* disable: C4127: conditional expression is constant */
#else
#ifdef __GNUC__
#define FORCE_INLINE static inline __attribute__((always_inline))
#else
#define FORCE_INLINE static inline
#endif
#endif

#ifdef _MSC_VER /* Visual Studio */
#define lz4_bswap16(x) _byteswap_ushort(x)
#else
#define lz4_bswap16(x) ((unsigned short int)((((x) >> 8) & 0xffu) | (((x)&0xffu) << 8)))
#endif

#define GCC_VERSION (__GNUC__ * 100 + __GNUC_MINOR__)

#if (GCC_VERSION >= 302) || (__INTEL_COMPILER >= 800) || defined(__clang__)
#define expect(expr, value) (__builtin_expect((expr), (value)))
#else
#define expect(expr, value) (expr)
#endif

#define likely(expr)   expect((expr) != 0, 1)
#define unlikely(expr) expect((expr) != 0, 0)


/**************************************
   Memory routines
**************************************/
//#include <stdlib.h>   /* malloc, calloc, free */
#define ALLOCATOR(n, s) calloc(n, s)
#define FREEMEM         free
#include <string.h> /* memset, memcpy */
#define MEM_INIT memset


/**************************************
   Includes
**************************************/
#include "lz4.h"


/**************************************
   Basic Types
**************************************/
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) /* C99 */
#include <stdint.h>
typedef uint8_t BYTE;
typedef uint16_t U16;
typedef uint32_t U32;
typedef int32_t S32;
typedef uint64_t U64;
#else
typedef unsigned char BYTE;
typedef unsigned short U16;
typedef unsigned int U32;
typedef signed int S32;
typedef unsigned long long U64;
#endif

#if defined(__GNUC__) && !defined(LZ4_FORCE_UNALIGNED_ACCESS)
#define _PACKED __attribute__((packed))
#else
#define _PACKED
#endif

#if !defined(LZ4_FORCE_UNALIGNED_ACCESS) && !defined(__GNUC__)
#if defined(__IBMC__) || defined(__SUNPRO_C) || defined(__SUNPRO_CC)
#pragma pack(1)
#else
#pragma pack(push, 1)
#endif
#endif

typedef struct
{
    U16 v;
} _PACKED U16_S;
typedef struct
{
    U32 v;
} _PACKED U32_S;
typedef struct
{
    U64 v;
} _PACKED U64_S;
typedef struct
{
    size_t v;
} _PACKED size_t_S;

#if !defined(LZ4_FORCE_UNALIGNED_ACCESS) && !defined(__GNUC__)
#if defined(__SUNPRO_C) || defined(__SUNPRO_CC)
#pragma pack(0)
#else
#pragma pack(pop)
#endif
#endif

#define A16(x)   (((U16_S *)(x))->v)
#define A32(x)   (((U32_S *)(x))->v)
#define A64(x)   (((U64_S *)(x))->v)
#define AARCH(x) (((size_t_S *)(x))->v)


/**************************************
   Constants
**************************************/
#define LZ4_HASHLOG   (MEMORY_USAGE - 2)
#define HASHTABLESIZE (1 << MEMORY_USAGE)
#define HASHNBCELLS4  (1 << LZ4_HASHLOG)

#define MINMATCH 4

#define COPYLENGTH   8
#define LASTLITERALS 5
#define MFLIMIT      (COPYLENGTH + MINMATCH)
// static const int LZ4_minLength = (MFLIMIT+1);

#define KB *(1U << 10)
#define MB *(1U << 20)
#define GB *(1U << 30)

#define LZ4_64KLIMIT ((64 KB) + (MFLIMIT - 1))
#define SKIPSTRENGTH 6 /* Increasing this value will make the compression run slower on incompressible data */

#define MAXD_LOG     16
#define MAX_DISTANCE ((1 << MAXD_LOG) - 1)

#define ML_BITS  4
#define ML_MASK  ((1U << ML_BITS) - 1)
#define RUN_BITS (8 - ML_BITS)
#define RUN_MASK ((1U << RUN_BITS) - 1)


/**************************************
   Structures and local types
**************************************/
typedef struct
{
    U32 hashTable[HASHNBCELLS4];
    const BYTE *bufferStart;
    const BYTE *base;
    const BYTE *nextBlock;
} LZ4_Data_Structure;

typedef enum { notLimited = 0,
               limited = 1 } limitedOutput_directive;
typedef enum { byPtr,
               byU32,
               byU16 } tableType_t;

typedef enum { noPrefix = 0,
               withPrefix = 1 } prefix64k_directive;

typedef enum { endOnOutputSize = 0,
               endOnInputSize = 1 } endCondition_directive;
typedef enum { full = 0,
               partial = 1 } earlyEnd_directive;


/**************************************
   Architecture-specific macros
**************************************/
#define STEPSIZE sizeof(size_t)
#define LZ4_COPYSTEP(d, s)   \
    {                        \
        AARCH(d) = AARCH(s); \
        d += STEPSIZE;       \
        s += STEPSIZE;       \
    }
#define LZ4_COPY8(d, s)         \
    {                           \
        LZ4_COPYSTEP(d, s);     \
        if (STEPSIZE < 8)       \
            LZ4_COPYSTEP(d, s); \
    }

#if (defined(LZ4_BIG_ENDIAN) && !defined(BIG_ENDIAN_NATIVE_BUT_INCOMPATIBLE))
#define LZ4_READ_LITTLEENDIAN_16(d, s, p) \
    {                                     \
        U16 v = A16(p);                   \
        v = lz4_bswap16(v);               \
        d = (s)-v;                        \
    }
#define LZ4_WRITE_LITTLEENDIAN_16(p, i) \
    {                                   \
        U16 v = (U16)(i);               \
        v = lz4_bswap16(v);             \
        A16(p) = v;                     \
        p += 2;                         \
    }
#else /* Little Endian */
#define LZ4_READ_LITTLEENDIAN_16(d, s, p) \
    {                                     \
        d = (s)-A16(p);                   \
    }
#define LZ4_WRITE_LITTLEENDIAN_16(p, v) \
    {                                   \
        A16(p) = v;                     \
        p += 2;                         \
    }
#endif


/**************************************
   Macros
**************************************/
#if LZ4_ARCH64 || !defined(__GNUC__)
#define LZ4_WILDCOPY(d, s, e) \
    {                         \
        do {                  \
            LZ4_COPY8(d, s)   \
        } while (d < e);      \
    } /* at the end, d>=e; */
#else
#define LZ4_WILDCOPY(d, s, e)           \
    {                                   \
        if (likely(e - d <= 8))         \
            LZ4_COPY8(d, s)             \
            else do { LZ4_COPY8(d, s) } \
        while (d < e)                   \
            ;                           \
    }
#endif
#define LZ4_SECURECOPY(d, s, e)    \
    {                              \
        if (d < e)                 \
            LZ4_WILDCOPY(d, s, e); \
    }

/****************************
   Decompression functions
****************************/
/*
 * This generic decompression function cover all use cases.
 * It shall be instanciated several times, using different sets of directives
 * Note that it is essential this generic function is really inlined,
 * in order to remove useless branches during compilation optimisation.
 */
FORCE_INLINE int LZ4_decompress_generic(
    const char *source,
    char *dest,
    int inputSize,
    int outputSize, /* If endOnInput==endOnInputSize, this value is the max size of Output Buffer. */

    int endOnInput,      /* endOnOutputSize, endOnInputSize */
    int prefix64k,       /* noPrefix, withPrefix */
    int partialDecoding, /* full, partial */
    int targetOutputSize /* only used if partialDecoding==partial */
)
{
    /* Local Variables */
    const BYTE *restrict ip = (const BYTE *)source;
    const BYTE *ref;
    const BYTE *const iend = ip + inputSize;

    BYTE *op = (BYTE *)dest;
    BYTE *const oend = op + outputSize;
    BYTE *cpy;
    BYTE *oexit = op + targetOutputSize;

    /*const size_t dec32table[] = {0, 3, 2, 3, 0, 0, 0, 0};   / static reduces speed for LZ4_decompress_safe() on GCC64 */
    const size_t dec32table[] = {4 - 0, 4 - 3, 4 - 2, 4 - 3, 4 - 0, 4 - 0, 4 - 0, 4 - 0}; /* static reduces speed for LZ4_decompress_safe() on GCC64 */
    static const size_t dec64table[] = {0, 0, 0, (size_t)-1, 0, 1, 2, 3};


    /* Special cases */
    if ((partialDecoding) && (oexit > oend - MFLIMIT))
        oexit = oend - MFLIMIT; /* targetOutputSize too high => decode everything */
    if ((endOnInput) && (unlikely(outputSize == 0)))
        return ((inputSize == 1) && (*ip == 0)) ? 0 : -1; /* Empty output buffer */
    if ((!endOnInput) && (unlikely(outputSize == 0)))
        return (*ip == 0 ? 1 : -1);


    /* Main Loop */
    while (1) {
        unsigned token;
        size_t length;

        /* get runlength */
        token = *ip++;
        if ((length = (token >> ML_BITS)) == RUN_MASK) {
            unsigned s = 255;
            while (((endOnInput) ? ip < iend : 1) && (s == 255)) {
                s = *ip++;
                length += s;
            }
        }

        /* copy literals */
        cpy = op + length;
        if (((endOnInput) && ((cpy > (partialDecoding ? oexit : oend - MFLIMIT)) || (ip + length > iend - (2 + 1 + LASTLITERALS)))) || ((!endOnInput) && (cpy > oend - COPYLENGTH))) {
            if (partialDecoding) {
                if (cpy > oend)
                    goto _output_error; /* Error : write attempt beyond end of output buffer */
                if ((endOnInput) && (ip + length > iend))
                    goto _output_error; /* Error : read attempt beyond end of input buffer */
            } else {
                if ((!endOnInput) && (cpy != oend))
                    goto _output_error; /* Error : block decoding must stop exactly there */
                if ((endOnInput) && ((ip + length != iend) || (cpy > oend)))
                    goto _output_error; /* Error : input must be consumed */
            }
            memcpy(op, ip, length);
            ip += length;
            op += length;
            break; /* Necessarily EOF, due to parsing restrictions */
        }
        LZ4_WILDCOPY(op, ip, cpy);
        ip -= (op - cpy);
        op = cpy;

        /* get offset */
        LZ4_READ_LITTLEENDIAN_16(ref, cpy, ip);
        ip += 2;
        if ((prefix64k == noPrefix) && (unlikely(ref < (BYTE *const)dest)))
            goto _output_error; /* Error : offset outside destination buffer */

        /* get matchlength */
        if ((length = (token & ML_MASK)) == ML_MASK) {
            while ((!endOnInput) || (ip < iend - (LASTLITERALS + 1))) /* Ensure enough bytes remain for LASTLITERALS + token */
            {
                unsigned s = *ip++;
                length += s;
                if (s == 255)
                    continue;
                break;
            }
        }

        /* copy repeated sequence */
        if (unlikely((op - ref) < (int)STEPSIZE)) {
            const size_t dec64 = dec64table[(sizeof(void *) == 4) ? 0 : op - ref];
            op[0] = ref[0];
            op[1] = ref[1];
            op[2] = ref[2];
            op[3] = ref[3];
            /*op += 4, ref += 4; ref -= dec32table[op-ref];
            A32(op) = A32(ref);
            op += STEPSIZE-4; ref -= dec64;*/
            ref += dec32table[op - ref];
            A32(op + 4) = A32(ref);
            op += STEPSIZE;
            ref -= dec64;
        } else {
            LZ4_COPYSTEP(op, ref);
        }
        cpy = op + length - (STEPSIZE - 4);

        if (unlikely(cpy > oend - COPYLENGTH - (STEPSIZE - 4))) {
            if (cpy > oend - LASTLITERALS)
                goto _output_error; /* Error : last 5 bytes must be literals */
            LZ4_SECURECOPY(op, ref, (oend - COPYLENGTH));
            while (op < cpy)
                *op++ = *ref++;
            op = cpy;
            continue;
        }
        LZ4_WILDCOPY(op, ref, cpy);
        op = cpy; /* correction */
    }

    /* end of decoding */
    if (endOnInput)
        return (int)(((char *)op) - dest); /* Nb of output bytes decoded */
    else
        return (int)(((char *)ip) - source); /* Nb of input bytes read */

    /* Overflow error detected */
_output_error:
    return (int)(-(((char *)ip) - source)) - 1;
}

int LZ4_decompress_fast(const char *source, char *dest, int outputSize)
{
#ifdef _MSC_VER /* This version is faster with Visual */
    return LZ4_decompress_generic(source, dest, 0, outputSize, endOnOutputSize, noPrefix, full, 0);
#else
    return LZ4_decompress_generic(source, dest, 0, outputSize, endOnOutputSize, withPrefix, full, 0);
#endif
}