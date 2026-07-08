/*
  LZ4cli - LZ4 Command Line Interface
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
  It is not part of LZ4 compression library, it is a user program of the LZ4 library.
  The license of LZ4 library is BSD.
  The license of xxHash library is BSD.
  The license of this compression CLI program is GPLv2.
*/


/*-************************************
*  Compiler options
**************************************/
#ifdef _MSC_VER    /* Visual Studio */
#  pragma warning(disable : 4127)    /* disable: C4127: conditional expression is constant */
#endif


/****************************
*  Includes
*****************************/
#include "platform.h" /* Compiler options, IS_CONSOLE */
#include "util.h"     /* UTIL_HAS_CREATEFILELIST, UTIL_createFileList */
#include <stdio.h>    /* fprintf, getchar */
#include <stdlib.h>   /* exit, calloc, free */
#include <string.h>   /* strcmp, strlen */
#include "lz4conf.h"  /* compile-time constants */
#include "bench.h"    /* BMK_benchFile, BMK_SetNbIterations, BMK_SetBlocksize, BMK_SetPause */
#include "lz4io.h"    /* LZ4IO_compressFilename, LZ4IO_decompressFilename, LZ4IO_compressMultipleFilenames */
#include "lz4hc.h"    /* LZ4HC_CLEVEL_MAX */
#include "lz4.h"      /* LZ4_VERSION_STRING */


/*****************************
*  Constants
******************************/
#if LZ4IO_MULTITHREAD
# define IO_MT "multithread"
#else
# define IO_MT "single-thread"
#endif
#define COMPRESSOR_NAME "lz4"
#define AUTHOR "Yann Collet"
#define WELCOME_MESSAGE "*** %s v%s %i-bit %s, by %s ***\n", COMPRESSOR_NAME, LZ4_versionString(), (int)(sizeof(void*)*8), IO_MT, AUTHOR
#define LZ4_EXTENSION ".lz4"
#define LZ4CAT "lz4cat"
#define UNLZ4 "unlz4"
#define LZ4_LEGACY "lz4c"
static int g_lz4c_legacy_commands = 0;

#define KB *(1U<<10)
#define MB *(1U<<20)
#define GB *(1U<<30)


/*-************************************
*  Macros
***************************************/
#define DISPLAYOUT(...)        fprintf(stdout, __VA_ARGS__)
#define DISPLAY(...)           fprintf(stderr, __VA_ARGS__)
#define DISPLAYLEVEL(l, ...)   do { if (displayLevel>=l) DISPLAY(__VA_ARGS__); } while (0)
static unsigned displayLevel = 2;   /* 0 : no display ; 1: errors only ; 2 : downgradable normal ; 3 : non-downgradable normal; 4 : + information */


/*-************************************
*  Errors and Messages
***************************************/
#ifndef DEBUG
#  define DEBUG 0
#endif
#define DEBUGOUTPUT(...) do { if (DEBUG) DISPLAY(__VA_ARGS__); } while (0)
#define END_PROCESS(error, ...)                                   \
do {                                                              \
    DEBUGOUTPUT("Error in %s, line %i : \n", __FILE__, __LINE__); \
    DISPLAYLEVEL(1, "Error %i : ", error);                        \
    DISPLAYLEVEL(1, __VA_ARGS__);                                 \
    DISPLAYLEVEL(1, "\n");                                        \
    exit(error);                                                  \
} while (0)

static void errorOut(const char* msg)
{
    DISPLAYLEVEL(1, "%s \n", msg); exit(1);
}


/*-************************************
*  Version modifiers
***************************************/
#define DEFAULT_COMPRESSOR   LZ4IO_compressFilename
#define DEFAULT_DECOMPRESSOR LZ4IO_decompressFilename
int LZ4IO_compressFilename_Legacy(const char* input_filename, const char* output_filename, int compressionlevel, const LZ4IO_prefs_t* prefs);   /* hidden function */
int LZ4IO_compressMultipleFilenames_Legacy(
                            const char** inFileNamesTable, int ifntSize,
                            const char* suffix,
                            int compressionLevel, const LZ4IO_prefs_t* prefs);

/*-***************************
*  Functions
*****************************/
static int usage(const char* exeName)
{
    DISPLAY( "Usage : \n");
    DISPLAY( "      %s [arg] [input] [output] \n", exeName);
    DISPLAY( "\n");
    DISPLAY( "input   : a filename \n");
    DISPLAY( "          with no FILE, or when FILE is - or %s, read standard input\n", stdinmark);
    DISPLAY( "Arguments : \n");
    DISPLAY( " -1     : fast compression (default) \n");
    DISPLAY( " -%2d    : slowest compression level \n", LZ4HC_CLEVEL_MAX);
#if LZ4IO_MULTITHREAD
    DISPLAY( " -T#    : use # threads for compression (default:%i==auto) \n", LZ4_NBWORKERS_DEFAULT);
#endif
    DISPLAY( " -d     : decompression (default for %s extension)\n", LZ4_EXTENSION);
    DISPLAY( " -f     : overwrite output without prompting \n");
    DISPLAY( " -k     : preserve source files(s)  (default) \n");
    DISPLAY( "--rm    : remove source file(s) after successful de/compression \n");
    DISPLAY( " -h/-H  : display help/long help and exit \n");
    return 0;
}

static int usage_advanced(const char* exeName)
{
    DISPLAY(WELCOME_MESSAGE);
    usage(exeName);
    DISPLAY( "\n");
    DISPLAY( "Advanced arguments :\n");
    DISPLAY( " -V     : display Version number and exit \n");
    DISPLAY( " -v     : verbose mode \n");
    DISPLAY( " -q     : suppress warnings; specify twice to suppress errors too\n");
    DISPLAY( " -c     : force write to standard output, even if it is the console\n");
    DISPLAY( " -t     : test compressed file integrity\n");
    DISPLAY( " -m     : multiple input files (implies automatic output filenames)\n");
#ifdef UTIL_HAS_CREATEFILELIST
    DISPLAY( " -r     : operate recursively on directories (sets also -m) \n");
#endif
    DISPLAY( " -l     : compress using Legacy format (Linux kernel compression)\n");
    DISPLAY( " -z     : force compression \n");
    DISPLAY( " -D FILE: use FILE as dictionary (compression & decompression)\n");
    DISPLAY( " -B#    : cut file into blocks of size # bytes [32+] \n");
    DISPLAY( "                     or predefined block size [4-7] (default: %i) \n", LZ4_BLOCKSIZEID_DEFAULT);
    DISPLAY( " -BI    : Block Independence (default) \n");
    DISPLAY( " -BD    : Block dependency (improves compression ratio) \n");
    DISPLAY( " -BX    : enable block checksum (default:disabled) \n");
    DISPLAY( "--no-frame-crc : disable stream checksum (default:enabled) \n");
    DISPLAY( "--content-size : compressed frame includes original size (default:not present)\n");
    DISPLAY( "--list FILE : lists information about .lz4 files (useful for files compressed with --content-size flag)\n");
    DISPLAY( "--[no-]sparse  : sparse mode (default:enabled on file, disabled on stdout)\n");
    DISPLAY( "--favor-decSpeed: compressed files decompress faster, but are less compressed \n");
    DISPLAY( "--fast[=#]: switch to ultra fast compression level (default: %i)\n", 1);
    DISPLAY( "--best  : same as -%d\n", LZ4HC_CLEVEL_MAX);
    DISPLAY( "Benchmark arguments : \n");
    DISPLAY( " -b#    : benchmark file(s), using # compression level (default : 1) \n");
    DISPLAY( " -e#    : test all compression levels from -bX to # (default : 1)\n");
    DISPLAY( " -i#    : minimum evaluation time in seconds (default : 3s) \n");
    if (g_lz4c_legacy_commands) {
        DISPLAY( "Legacy arguments : \n");
        DISPLAY( " -c0    : fast compression \n");
        DISPLAY( " -c1    : high compression \n");
        DISPLAY( " -c2,-hc: very high compression \n");
        DISPLAY( " -y     : overwrite output without prompting \n");
    }
    return 0;
}

static int usage_longhelp(const char* exeName)
{
    usage_advanced(exeName);
    DISPLAY( "\n");
    DISPLAY( "****************************\n");
    DISPLAY( "***** Advanced comment *****\n");
    DISPLAY( "****************************\n");
    DISPLAY( "\n");
    DISPLAY( "Which values can [output] have ? \n");
    DISPLAY( "---------------------------------\n");
    DISPLAY( "[output] : a filename \n");
    DISPLAY( "          '%s', or '-' for standard output (pipe mode)\n", stdoutmark);
    DISPLAY( "          '%s' to discard output (test mode) \n", NULL_OUTPUT);
    DISPLAY( "[output] can be left empty. In this case, it receives the following value :\n");
    DISPLAY( "          - if stdout is not the console, then [output] = stdout \n");
    DISPLAY( "          - if stdout is console : \n");
    DISPLAY( "               + for compression, output to filename%s \n", LZ4_EXTENSION);
    DISPLAY( "               + for decompression, output to filename without '%s'\n", LZ4_EXTENSION);
    DISPLAY( "                    > if input filename has no '%s' extension : error \n", LZ4_EXTENSION);
    DISPLAY( "\n");
    DISPLAY( "Compression levels : \n");
    DISPLAY( "---------------------\n");
    DISPLAY( "-0  => Default level, identical to -%u \n", LZ4_CLEVEL_DEFAULT);
    DISPLAY( "-1  => Fast compression \n");
    DISPLAY( "-2 .. -%d => High compression; higher number == more compression but slower\n", LZ4HC_CLEVEL_MAX);
    DISPLAY( "--best    => Highest available compression level (-%d) \n", LZ4HC_CLEVEL_MAX);
    DISPLAY( "--fast=#  => Faster compression; higher number == faster but compress less\n");
    DISPLAY( "\n");
    DISPLAY( "stdin, stdout and the console : \n");
    DISPLAY( "--------------------------------\n");
    DISPLAY( "To protect the console from binary flooding (bad argument mistake)\n");
    DISPLAY( "%s will refuse to read from console, or write to console \n", exeName);
    DISPLAY( "except if '-c' command is specified, to force output to console \n");
    DISPLAY( "\n");
    DISPLAY( "Simple example :\n");
    DISPLAY( "----------------\n");
    DISPLAY( "1 : compress 'filename' fast, using default output name 'filename.lz4'\n");
    DISPLAY( "          %s filename\n", exeName);
    DISPLAY( "\n");
    DISPLAY( "Short arguments can be aggregated. For example :\n");
    DISPLAY( "----------------------------------\n");
    DISPLAY( "2 : compress 'filename' in high compression mode, overwrite output if exists\n");
    DISPLAY( "          %s -9 -f filename \n", exeName);
    DISPLAY( "    is equivalent to :\n");
    DISPLAY( "          %s -9f filename \n", exeName);
    DISPLAY( "\n");
    DISPLAY( "%s can be used in 'pure pipe mode'. For example :\n", exeName);
    DISPLAY( "-------------------------------------\n");
    DISPLAY( "3 : compress data stream from 'generator', send result to 'consumer'\n");
    DISPLAY( "          generator | %s | consumer \n", exeName);
    if (g_lz4c_legacy_commands) {
        DISPLAY( "\n");
        DISPLAY( "***** Warning  ***** \n");
        DISPLAY( "Legacy arguments take precedence. Therefore : \n");
        DISPLAY( "--------------------------------- \n");
        DISPLAY( "          %s -hc filename \n", exeName);
        DISPLAY( "means 'compress filename in high compression mode' \n");
        DISPLAY( "It is not equivalent to : \n");
        DISPLAY( "          %s -h -c filename \n", exeName);
        DISPLAY( "which displays help text and exits \n");
    }
    return 0;
}

static int badusage(const char* exeName)
{
    DISPLAYLEVEL(1, "Incorrect parameters\n");
    if (displayLevel >= 1) usage(exeName);
    exit(1);
}


static void waitEnter(void)
{
    DISPLAY("Press enter to continue...\n");
    (void)getchar();
}

static const char* lastNameFromPath(const char* path)
{
    const char* name = path;
    if (strrchr(name, '/')) name = strrchr(name, '/') + 1;
    if (strrchr(name, '\\')) name = strrchr(name, '\\') + 1; /* windows */
    return name;
}

/*! exeNameMatch() :
    @return : a non-zero value if exeName matches test, excluding the extension
   */
static int exeNameMatch(const char* exeName, const char* test)
{
    return !strncmp(exeName, test, strlen(test)) &&
        (exeName[strlen(test)] == '\0' || exeName[strlen(test)] == '.');
}

/*! readU32FromChar() :
 * @return : unsigned integer value read from input in `char` format
 *  allows and interprets K, KB, KiB, M, MB and MiB suffix.
 *  Will also modify `*stringPtr`, advancing it to position where it stopped reading.
 *  Note : function result can overflow if digit string > MAX_UINT */
static unsigned readU32FromChar(const char** stringPtr)
{
    unsigned result = 0;
    while ((**stringPtr >='0') && (**stringPtr <='9')) {
        result *= 10;
        result += (unsigned)(**stringPtr - '0');
        (*stringPtr)++ ;
    }
    if ((**stringPtr=='K') || (**stringPtr=='M')) {
        result <<= 10;
        if (**stringPtr=='M') result <<= 10;
        (*stringPtr)++ ;
        if (**stringPtr=='i') (*stringPtr)++;
        if (**stringPtr=='B') (*stringPtr)++;
    }
    return result;
}

#define CLEAN_RETURN(i) { operationResult = (i); goto _cleanup; }

#define NEXT_FIELD(ptr) {         \
    if (*argument == '=') {       \
        ptr = ++argument;         \
        argument += strlen(ptr);  \
    } else {                      \
        argNb++;                  \
        if (argNb >= argCount) {  \
            DISPLAYLEVEL(1, "error: missing command argument \n"); \
            CLEAN_RETURN(1);      \
        }                         \
        ptr = argv[argNb];        \
        assert(ptr != NULL);      \
        if (ptr[0]=='-') {        \
            DISPLAYLEVEL(1, "error: command cannot be separated from its argument by another command \n"); \
            CLEAN_RETURN(1);      \
}   }   }

#define NEXT_UINT32(val32) {      \
    const char* __nb;             \
    NEXT_FIELD(__nb);             \
    val32 = readU32FromChar(&__nb); \
    if(*__nb != 0) {              \
        errorOut("error: only numeric values with optional suffixes K, KB, KiB, M, MB, MiB are allowed"); \
    }                             \
}

/** longCommandWArg() :
 *  check if *stringPtr is the same as longCommand.
 *  If yes, @return 1 and advances *stringPtr to the position which immediately follows longCommand.
 * @return 0 and doesn't modify *stringPtr otherwise.
 */
static int longCommandWArg(const char** stringPtr, const char* longCommand)
{
    size_t const comSize = strlen(longCommand);
    int const result = !strncmp(*stringPtr, longCommand, comSize);
    if (result) *stringPtr += comSize;
    return result;
}

typedef enum { om_auto, om_compress, om_decompress, om_test, om_bench, om_list } operationMode_e;

/** determineOpMode() :
 *  auto-determine operation mode, based on input filename extension
 *  @return `om_decompress` if input filename has .lz4 extension and `om_compress` otherwise.
 */
static operationMode_e determineOpMode(const char* inputFilename)
{
    size_t const inSize  = strlen(inputFilename);
    size_t const extSize = strlen(LZ4_EXTENSION);
    size_t const extStart= (inSize > extSize) ? inSize-extSize : 0;
    if (!strcmp(inputFilename+extStart, LZ4_EXTENSION)) return om_decompress;
    else return om_compress;
}

#define ENV_NBTHREADS "LZ4_NBWORKERS"

static unsigned init_nbWorkers(void)
{
    const char* const env = getenv(ENV_NBTHREADS);
    if (env != NULL) {
        const char* ptr = env;
        if ((*ptr>='0') && (*ptr<='9')) {
            return readU32FromChar(&ptr);
        }
        DISPLAYLEVEL(2, "Ignore environment variable setting %s=%s: not a valid unsigned value \n", ENV_NBTHREADS, env);
    }
    return LZ4_NBWORKERS_DEFAULT;
}

#define ENV_CLEVEL "LZ4_CLEVEL"

static int init_cLevel(void)
{
    const char* const env = getenv(ENV_CLEVEL);
    if (env != NULL) {
        const char* ptr = env;
        if ((*ptr>='0') && (*ptr<='9')) {
            return (int)readU32FromChar(&ptr);
        }
        DISPLAYLEVEL(2, "Ignore environment variable setting %s=%s: not a valid unsigned value \n", ENV_CLEVEL, env);
    }
    return LZ4_CLEVEL_DEFAULT;
}

int main(int argCount, const char** argv)
{
    int argNb,
        cLevel=init_cLevel(),
        cLevelLast=-10000,
        legacy_format=0,
        forceStdout=0,
        forceOverwrite=0,
        main_pause=0,
        multiple_inputs=0,
        all_arguments_are_files=0,
        operationResult=0,
        contentSizeFlag=0;
    unsigned nbWorkers = init_nbWorkers();
    operationMode_e mode = om_auto;
    const char* input_filename = NULL;
    const char* output_filename= NULL;
    const char* dictionary_filename = NULL;
    char* dynNameSpace = NULL;
    const char** inFileNames = (const char**)calloc((size_t)argCount, sizeof(char*));
    unsigned ifnIdx=0;
    LZ4IO_prefs_t* const prefs = LZ4IO_defaultPreferences();
    const char nullOutput[] = NULL_OUTPUT;
    const char extension[] = LZ4_EXTENSION;
    size_t blockSize = LZ4IO_setBlockSizeID(prefs, LZ4_BLOCKSIZEID_DEFAULT);
    const char* const exeName = lastNameFromPath(argv[0]);
    char* fileNamesBuf = NULL;
#ifdef UTIL_HAS_CREATEFILELIST
    unsigned fileNamesNb, recursive=0;
#endif

    /* Init */
    if (inFileNames==NULL) {
        DISPLAY("Allocation error : not enough memory \n");
        operationResult = 1;
        goto _cleanup;
    }
    inFileNames[0] = stdinmark;
    LZ4IO_setOverwrite(prefs, 0);

    /* predefined behaviors, based on binary/link name */
    if (exeNameMatch(exeName, LZ4CAT)) {
        mode = om_decompress;
        LZ4IO_setOverwrite(prefs, 1);
        LZ4IO_setPassThrough(prefs, 1);
        LZ4IO_setRemoveSrcFile(prefs, 0);
        forceStdout=1;
        output_filename=stdoutmark;
        displayLevel=1;
        multiple_inputs=1;
    }
    if (exeNameMatch(exeName, UNLZ4)) { mode = om_decompress; }
    if (exeNameMatch(exeName, LZ4_LEGACY)) { g_lz4c_legacy_commands=1; }

    /* command switches */
    for(argNb=1; argNb<argCount; argNb++) {
        const char* argument = argv[argNb];

        if(!argument) continue;   /* Protection if argument empty */

        /* Short commands (note : aggregated short commands are allowed) */
        if (!all_arguments_are_files && argument[0]=='-') {
            /* '-' means stdin/stdout */
            if (argument[1]==0) {
                if (!input_filename) input_filename=stdinmark;
                else output_filename=stdoutmark;
                continue;
            }

            /* long commands (--long-word) */
            if (argument[1]=='-') {
                if (!strcmp(argument,  "--")) { all_arguments_are_files = 1; continue; }
                if (!strcmp(argument,  "--compress")) { mode = om_compress; continue; }
                if ( (!strcmp(argument, "--decompress"))
                  || (!strcmp(argument, "--uncompress"))) {
                      if (mode != om_bench) mode = om_decompress;
                      BMK_setDecodeOnlyMode(1);
                      continue;
                  }
                if (!strcmp(argument,  "--multiple")) { multiple_inputs = 1; continue; }
                if (!strcmp(argument,  "--test")) { mode = om_test; continue; }
                if (!strcmp(argument,  "--force")) { LZ4IO_setOverwrite(prefs, 1); continue; }
                if (!strcmp(argument,  "--no-force")) { LZ4IO_setOverwrite(prefs, 0); continue; }
                if ((!strcmp(argument, "--stdout"))
                    || (!strcmp(argument, "--to-stdout"))) { forceStdout=1; output_filename=stdoutmark; continue; }
                if (!strcmp(argument,  "--frame-crc")) { LZ4IO_setStreamChecksumMode(prefs, 1); BMK_skipChecksums(0); continue; }
                if (!strcmp(argument,  "--no-frame-crc")) { LZ4IO_setStreamChecksumMode(prefs, 0); BMK_skipChecksums(1); continue; }
                if (!strcmp(argument,  "--no-crc")) { LZ4IO_setStreamChecksumMode(prefs, 0); LZ4IO_setBlockChecksumMode(prefs, 0); BMK_skipChecksums(1); continue; }
                if (!strcmp(argument,  "--content-size")) { contentSizeFlag = 1; LZ4IO_setContentSize(prefs, 1); continue;}
                if (!strcmp(argument,  "--no-content-size")) { LZ4IO_setContentSize(prefs, 0); continue; }
                if (!strcmp(argument,  "--list")) { mode = om_list; multiple_inputs = 1; continue; }
                if (!strcmp(argument,  "--sparse")) { LZ4IO_setSparseFile(prefs, 2); continue; }
                if (!strcmp(argument,  "--no-sparse")) { LZ4IO_setSparseFile(prefs, 0); continue; }
                if (!strcmp(argument,  "--favor-decSpeed")) { LZ4IO_favorDecSpeed(prefs, 1); continue; }
                if (!strcmp(argument,  "--verbose")) { displayLevel++; continue; }
                if (!strcmp(argument,  "--quiet")) { if (displayLevel) displayLevel--; continue; }
                if (!strcmp(argument,  "--version")) { DISPLAYOUT(WELCOME_MESSAGE); goto _cleanup; }
                if (!strcmp(argument,  "--help")) { usage_advanced(exeName); goto _cleanup; }
                if (!strcmp(argument,  "--keep")) { LZ4IO_setRemoveSrcFile(prefs, 0); continue; }   /* keep source file (default) */
                if (!strcmp(argument,  "--rm")) { LZ4IO_setRemoveSrcFile(prefs, 1); continue; }

                if (longCommandWArg(&argument, "--threads")) {
                    NEXT_UINT32(nbWorkers);
                    continue;
                }
                if (longCommandWArg(&argument, "--fast")) {
                    /* Parse optional acceleration factor */
                    if (*argument == '=') {
                        U32 fastLevel;
                        ++argument;
                        fastLevel = readU32FromChar(&argument);
                        if (fastLevel) {
                            cLevel = -(int)fastLevel;
                        } else {
                            badusage(exeName);
                        }
                    } else if (*argument != 0) {
                        /* Invalid character following --fast */
                        badusage(exeName);
                    } else {
                        cLevel = -1;  /* default for --fast */
                    }
                    continue;
                }

                /* For gzip(1) compatibility */
                if (!strcmp(argument,  "--best")) { cLevel=LZ4HC_CLEVEL_MAX; continue; }
            }

            while (argument[1]!=0) {
                argument ++;

                if (g_lz4c_legacy_commands) {
                    /* Legacy commands (-c0, -c1, -hc, -y) */
                    if (!strcmp(argument,  "c0")) { cLevel=0; argument++; continue; }  /* -c0 (fast compression) */
                    if (!strcmp(argument,  "c1")) { cLevel=9; argument++; continue; }  /* -c1 (high compression) */
                    if (!strcmp(argument,  "c2")) { cLevel=12; argument++; continue; } /* -c2 (very high compression) */
                    if (!strcmp(argument,  "hc")) { cLevel=12; argument++; continue; } /* -hc (very high compression) */
                    if (!strcmp(argument,  "y"))  { LZ4IO_setOverwrite(prefs, 1); continue; } /* -y (answer 'yes' to overwrite permission) */
                }

                if ((*argument>='0') && (*argument<='9')) {
                    cLevel = (int)readU32FromChar(&argument);
                    argument--;
                    continue;
                }


                switch(argument[0])
                {
                    /* Display help */
                case 'V': DISPLAYOUT(WELCOME_MESSAGE); goto _cleanup;   /* Version */
                case 'h': usage_advanced(exeName); goto _cleanup;
                case 'H': usage_longhelp(exeName); goto _cleanup;

                case 'e':
                    argument++;
                    cLevelLast = (int)readU32FromChar(&argument);
                    argument--;
                    break;

                    /* Compression (default) */
                case 'z': mode = om_compress; break;

                    /* Modify Nb Worker threads (compression only) */
                case 'T':
                    {   argument++;
                        nbWorkers = readU32FromChar(&argument);
                        argument--;
                    }
                    break;

                case 'D':
                    if (argument[1] == '\0') {
                        /* path is next arg */
                        if (argNb + 1 == argCount) {
                            /* there is no next arg */
                            badusage(exeName);
                        }
                        dictionary_filename = argv[++argNb];
                    } else {
                        /* path follows immediately */
                        dictionary_filename = argument + 1;
                    }
                    /* skip to end of argument so that we jump to parsing next argument */
                    argument += strlen(argument) - 1;
                    break;

                    /* Use Legacy format (ex : Linux kernel compression) */
                case 'l': legacy_format = 1; blockSize = 8 MB; break;

                    /* Decoding */
                case 'd':
                    if (mode != om_bench) mode = om_decompress;
                    BMK_setDecodeOnlyMode(1);
                    break;

                    /* Force stdout, even if stdout==console */
                case 'c':
                  forceStdout=1;
                  output_filename=stdoutmark;
                  LZ4IO_setPassThrough(prefs, 1);
                  break;

                    /* Test integrity */
                case 't': mode = om_test; break;

                    /* Overwrite */
                case 'f': forceOverwrite=1; LZ4IO_setOverwrite(prefs, 1); break;

                    /* Verbose mode */
                case 'v': displayLevel++; break;

                    /* Quiet mode */
                case 'q': if (displayLevel) displayLevel--; break;

                    /* keep source file (default anyway, so useless) (for xz/lzma compatibility) */
                case 'k': LZ4IO_setRemoveSrcFile(prefs, 0); break;

                    /* Modify Block Properties */
                case 'B':
                    while (argument[1]!=0) {
                        int exitBlockProperties=0;
                        switch(argument[1])
                        {
                        case 'D': LZ4IO_setBlockMode(prefs, LZ4IO_blockLinked); argument++; break;
                        case 'I': LZ4IO_setBlockMode(prefs, LZ4IO_blockIndependent); argument++; break;
                        case 'X': LZ4IO_setBlockChecksumMode(prefs, 1); argument ++; break;   /* disabled by default */
                        default :
                            if (argument[1] < '0' || argument[1] > '9') {
                                exitBlockProperties=1;
                                break;
                            } else {
                                unsigned B;
                                argument++;
                                B = readU32FromChar(&argument);
                                argument--;
                                if (B < 4) badusage(exeName);
                                if (B <= 7) {
                                    blockSize = LZ4IO_setBlockSizeID(prefs, B);
                                    BMK_setBlockSize(blockSize);
                                    DISPLAYLEVEL(2, "using blocks of size %u KB \n", (U32)(blockSize>>10));
                                } else {
                                    if (B < 32) badusage(exeName);
                                    blockSize = LZ4IO_setBlockSize(prefs, B);
                                    BMK_setBlockSize(blockSize);
                                    if (blockSize >= 1024) {
                                        DISPLAYLEVEL(2, "using blocks of size %u KB \n", (U32)(blockSize>>10));
                                    } else {
                                        DISPLAYLEVEL(2, "using blocks of size %u bytes \n", (U32)(blockSize));
                                    }
                                }
                                break;
                            }
                        }
                        if (exitBlockProperties) break;
                    }
                    break;

                    /* Benchmark */
                case 'b': mode = om_bench; multiple_inputs=1;
                    break;

                    /* hidden command : benchmark files, but do not fuse result */
                case 'S': BMK_setBenchSeparately(1);
                    break;

#ifdef UTIL_HAS_CREATEFILELIST
                    /* recursive */
                case 'r': recursive=1;
#endif
                    /* fall-through */
                    /* Treat non-option args as input files.  See https://code.google.com/p/lz4/issues/detail?id=151 */
                case 'm': multiple_inputs=1;
                    break;

                    /* Modify Nb Seconds (benchmark only) */
                case 'i':
                    {   unsigned iters;
                        argument++;
                        iters = readU32FromChar(&argument);
                        argument--;
                        BMK_setNotificationLevel(displayLevel);
                        BMK_setNbSeconds(iters);   /* notification if displayLevel >= 3 */
                    }
                    break;

                    /* Pause at the end (hidden option) */
                case 'p': main_pause=1; break;

                    /* Unrecognised command */
                default : badusage(exeName);
                }
            }
            continue;
        }

        /* Store in *inFileNames[] if -m is used. */
        if (multiple_inputs) { inFileNames[ifnIdx++] = argument; continue; }

        /* original cli logic : lz4 input output */
        /* First non-option arg is input_filename. */
        if (!input_filename) { input_filename = argument; continue; }

        /* Second non-option arg is output_filename */
        if (!output_filename) {
            output_filename = argument;
            if (!strcmp (output_filename, nullOutput)) output_filename = nulmark;
            continue;
        }

        /* 3rd+ non-option arg should not exist */
        DISPLAYLEVEL(1, "%s : %s won't be used ! Do you want multiple input files (-m) ? \n",
            forceOverwrite ? "Warning" : "Error",
            argument);
        if (!forceOverwrite) exit(1);
    }

    DISPLAYLEVEL(3, WELCOME_MESSAGE);
#ifdef _POSIX_C_SOURCE
    DISPLAYLEVEL(4, "_POSIX_C_SOURCE defined: %ldL\n", (long) _POSIX_C_SOURCE);
#endif
#ifdef _POSIX_VERSION
    DISPLAYLEVEL(4, "_POSIX_VERSION defined: %ldL\n", (long) _POSIX_VERSION);
#endif
#ifdef PLATFORM_POSIX_VERSION
    DISPLAYLEVEL(4, "PLATFORM_POSIX_VERSION defined: %ldL\n", (long) PLATFORM_POSIX_VERSION);
#endif
#ifdef _FILE_OFFSET_BITS
    DISPLAYLEVEL(5, "_FILE_OFFSET_BITS defined: %ldL\n", (long) _FILE_OFFSET_BITS);
#endif
#if !LZ4IO_MULTITHREAD
    if (nbWorkers > 1) {
        DISPLAYLEVEL(2, "warning: this executable doesn't support multithreading \n");
    }
#endif
    if ((mode == om_compress) || (mode == om_bench)) {
        DISPLAYLEVEL(4, "Blocks size : %u KB\n", (U32)(blockSize>>10));
    }

    if (multiple_inputs) {
        input_filename = inFileNames[0];
#ifdef UTIL_HAS_CREATEFILELIST
        if (recursive) {  /* at this stage, filenameTable is a list of paths, which can contain both files and directories */
            const char** extendedFileList = UTIL_createFileList(inFileNames, ifnIdx, &fileNamesBuf, &fileNamesNb);
            if (extendedFileList) {
                unsigned u;
                for (u=0; u<fileNamesNb; u++) DISPLAYLEVEL(4, "%u %s\n", u, extendedFileList[u]);
                free((void*)inFileNames);
                inFileNames = extendedFileList;
                ifnIdx = fileNamesNb;
        }   }
#endif
    }

    if (dictionary_filename) {
        if (!strcmp(dictionary_filename, stdinmark) && IS_CONSOLE(stdin)) {
            DISPLAYLEVEL(1, "refusing to read from a console\n");
            exit(1);
        }
        LZ4IO_setDictionaryFilename(prefs, dictionary_filename);
    }

    /* benchmark and test modes */
    if (mode == om_bench) {
        BMK_setNotificationLevel(displayLevel);
        operationResult = BMK_benchFiles(inFileNames, ifnIdx, cLevel, cLevelLast, dictionary_filename);
        goto _cleanup;
    }

    if (mode == om_test) {
        LZ4IO_setTestMode(prefs, 1);
        output_filename = nulmark;
        mode = om_decompress;   /* defer to decompress */
    }

    /* No input provided => use stdin */
    if (!input_filename) input_filename = stdinmark;

    /* Refuse to use the console as input */
    if (!strcmp(input_filename, stdinmark) && IS_CONSOLE(stdin) ) {
        DISPLAYLEVEL(1, "refusing to read from a console\n");
        exit(1);
    }

    if (!strcmp(input_filename, stdinmark)) {
        /* if input==stdin and no output defined, stdout becomes default output */
        if (!output_filename) output_filename = stdoutmark;
        if (contentSizeFlag){
            DISPLAYLEVEL(2, "warning : --content-size cannot determine size when used with stdin\n");
        }
    }

    /* No output filename ==> try to select one automatically (when possible) */
    if ((!output_filename) && (multiple_inputs==0)) {
        if (mode == om_auto) {  /* auto-determine compression or decompression, based on file extension */
            mode = determineOpMode(input_filename);
        }
        if (mode == om_compress) {   /* compression to file */
            size_t const l = strlen(input_filename);
            dynNameSpace = (char*)calloc(1,l+5);
            if (dynNameSpace==NULL) { perror(exeName); exit(1); }
            strcpy(dynNameSpace, input_filename);
            strcat(dynNameSpace, LZ4_EXTENSION);
            output_filename = dynNameSpace;
            DISPLAYLEVEL(2, "Compressed filename will be : %s \n", output_filename);
        }
        if (mode == om_decompress) {
            /* decompress to file (automatic output name only works if input filename has correct format extension) */
            size_t outl;
            size_t const inl = strlen(input_filename);
            dynNameSpace = (char*)calloc(1,inl+1);
            if (dynNameSpace==NULL) { perror(exeName); exit(1); }
            strcpy(dynNameSpace, input_filename);
            outl = inl;
            if (inl>4)
                while ((outl >= inl-4) && (input_filename[outl] ==  extension[outl-inl+4])) dynNameSpace[outl--]=0;
            if (outl != inl-5) { DISPLAYLEVEL(1, "Cannot determine an output filename \n"); badusage(exeName); }
            output_filename = dynNameSpace;
            DISPLAYLEVEL(2, "Decoding file %s \n", output_filename);
        }
    }

    if (mode == om_list) {
        if (!multiple_inputs) inFileNames[ifnIdx++] = input_filename;
    } else {
        if (!multiple_inputs) assert(output_filename != NULL);
    }
    /* when multiple_inputs==1, output_filename may simply be useless,
     * however, output_filename must be !NULL for next strcmp() tests */
    if (!output_filename) output_filename = "*\\dummy^!//";

    /* Check if output is defined as console; trigger an error in this case */
    if ( !strcmp(output_filename,stdoutmark)
      && mode != om_list
      && IS_CONSOLE(stdout)
      && !forceStdout) {
        DISPLAYLEVEL(1, "refusing to write to console without -c \n");
        exit(1);
    }
    /* Downgrade notification level in stdout and multiple file mode */
    if (!strcmp(output_filename,stdoutmark) && (displayLevel==2)) displayLevel=1;
    if ((multiple_inputs) && (displayLevel==2)) displayLevel=1;

    /* Auto-determine compression or decompression, based on file extension */
    if (mode == om_auto) {
        mode = determineOpMode(input_filename);
    }

    /* IO Stream/File */
    LZ4IO_setNotificationLevel((int)displayLevel);
    if (ifnIdx == 0) multiple_inputs = 0;
    if (mode == om_decompress) {
        if (multiple_inputs) {
            const char* dec_extension = LZ4_EXTENSION;
            if (!strcmp(output_filename, stdoutmark)) dec_extension = stdoutmark;
            if (!strcmp(output_filename, nulmark)) dec_extension = nulmark;
            assert(ifnIdx < INT_MAX);
            operationResult = LZ4IO_decompressMultipleFilenames(inFileNames, (int)ifnIdx, dec_extension, prefs);
        } else {
            operationResult = DEFAULT_DECOMPRESSOR(input_filename, output_filename, prefs);
        }
    } else if (mode == om_list){
        operationResult = LZ4IO_displayCompressedFilesInfo(inFileNames, ifnIdx);
    } else {   /* compression is default action */
#if LZ4IO_MULTITHREAD
        if (nbWorkers != 1) {
            if (nbWorkers==0)
                nbWorkers = (unsigned)LZ4IO_defaultNbWorkers();
            if (nbWorkers > LZ4_NBWORKERS_MAX) {
                DISPLAYLEVEL(3, "Requested %u threads too large => automatically reduced to %u \n",
                            nbWorkers, LZ4_NBWORKERS_MAX);
                nbWorkers = LZ4_NBWORKERS_MAX;
            } else {
                DISPLAYLEVEL(3, "Using %u threads for compression \n", nbWorkers);
            }
        }
        LZ4IO_setNbWorkers(prefs, (int)nbWorkers);
#endif
        if (legacy_format) {
            DISPLAYLEVEL(3, "! Generating LZ4 Legacy format (deprecated) ! \n");
            if(multiple_inputs){
                const char* const leg_extension = !strcmp(output_filename,stdoutmark) ? stdoutmark : LZ4_EXTENSION;
                operationResult = LZ4IO_compressMultipleFilenames_Legacy(inFileNames, (int)ifnIdx, leg_extension, cLevel, prefs);
            } else {
                operationResult = LZ4IO_compressFilename_Legacy(input_filename, output_filename, cLevel, prefs);
            }
        } else {
            if (multiple_inputs) {
                const char* const comp_extension = !strcmp(output_filename,stdoutmark) ? stdoutmark : LZ4_EXTENSION;
                assert(ifnIdx <= INT_MAX);
                operationResult = LZ4IO_compressMultipleFilenames(inFileNames, (int)ifnIdx, comp_extension, cLevel, prefs);
            } else {
                operationResult = DEFAULT_COMPRESSOR(input_filename, output_filename, cLevel, prefs);
    }   }   }

_cleanup:
    if (main_pause) waitEnter();
    free(dynNameSpace);
    free(fileNamesBuf);
    LZ4IO_freePreferences(prefs);
    free((void*)inFileNames);
    return operationResult;
}
