/*
    util.h - utility functions
    Copyright (c) Yann Collet. All rights reserved.

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
*/

#ifndef UTIL_H_MODULE
#define UTIL_H_MODULE

#if defined (__cplusplus)
extern "C" {
#endif



/*-****************************************
*  Dependencies
******************************************/
#include "platform.h"     /* PLATFORM_POSIX_VERSION */
#include <stddef.h>       /* size_t, ptrdiff_t */
#include <stdlib.h>       /* malloc */
#include <string.h>       /* strlen, strncpy */
#include <stdio.h>        /* fprintf, fileno */
#include <assert.h>
#include <sys/types.h>    /* stat, utime */
#include <sys/stat.h>     /* stat */
#if defined(_WIN32)
#  include <sys/utime.h>  /* utime */
#  include <io.h>         /* _chmod */
#else
#  include <unistd.h>     /* chown, stat */
# if PLATFORM_POSIX_VERSION < 200809L
#  include <utime.h>      /* utime */
# else
#  include <fcntl.h>      /* AT_FDCWD */
#  include <sys/stat.h>   /* for utimensat */
# endif
#endif
#include <time.h>         /* time */
#include <limits.h>       /* INT_MAX */
#include <errno.h>



/*-**************************************************************
*  Basic Types
*****************************************************************/
#if !defined (__VMS) && (defined (__cplusplus) || (defined (__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) /* C99 */) )
# include <stdint.h>
  typedef  uint8_t BYTE;
  typedef uint16_t U16;
  typedef  int16_t S16;
  typedef uint32_t U32;
  typedef  int32_t S32;
  typedef uint64_t U64;
  typedef  int64_t S64;
#else
  typedef unsigned char       BYTE;
  typedef unsigned short      U16;
  typedef   signed short      S16;
  typedef unsigned int        U32;
  typedef   signed int        S32;
  typedef unsigned long long  U64;
  typedef   signed long long  S64;
#endif


/* ************************************************************
* Avoid fseek()'s 2GiB barrier with MSVC, MacOS, *BSD, MinGW
***************************************************************/
#if defined(_MSC_VER) && (_MSC_VER >= 1400)
#   define UTIL_fseek _fseeki64
#elif !defined(__64BIT__) && (PLATFORM_POSIX_VERSION >= 200112L) /* No point defining Large file for 64 bit */
#  define UTIL_fseek fseeko
#elif defined(__MINGW32__) && defined(__MSVCRT__) && !defined(__STRICT_ANSI__) && !defined(__NO_MINGW_LFS)
#   define UTIL_fseek fseeko64
#else
#   define UTIL_fseek fseek
#endif

/*-****************************************
*  Local host Core counting
******************************************/
int UTIL_countCores(void);

/*-****************************************
*  Sleep functions: Windows - Posix - others
******************************************/
#if defined(_WIN32)
#  include <windows.h>
#  define SET_REALTIME_PRIORITY SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS)
#  define UTIL_sleep(s) Sleep(1000*s)
#  define UTIL_sleepMilli(milli) Sleep(milli)
#elif PLATFORM_POSIX_VERSION >= 0 /* Unix-like operating system */
#  include <unistd.h>
#  include <sys/resource.h> /* setpriority */
#  include <time.h>         /* clock_t, nanosleep, clock, CLOCKS_PER_SEC */
#  if defined(PRIO_PROCESS)
#    define SET_REALTIME_PRIORITY setpriority(PRIO_PROCESS, 0, -20)
#  else
#    define SET_REALTIME_PRIORITY /* disabled */
#  endif
#  define UTIL_sleep(s) sleep(s)
#  if (defined(__linux__) && (PLATFORM_POSIX_VERSION >= 199309L)) || (PLATFORM_POSIX_VERSION >= 200112L)  /* nanosleep requires POSIX.1-2001 */
#      define UTIL_sleepMilli(milli) { struct timespec t; t.tv_sec=0; t.tv_nsec=milli*1000000ULL; nanosleep(&t, NULL); }
#  else
#      define UTIL_sleepMilli(milli) /* disabled */
#  endif
#else
#  define SET_REALTIME_PRIORITY      /* disabled */
#  define UTIL_sleep(s)          /* disabled */
#  define UTIL_sleepMilli(milli) /* disabled */
#endif


/*-****************************************
*  stat() functions
******************************************/
#if defined(_MSC_VER)
#  define UTIL_TYPE_stat __stat64
#  define UTIL_stat _stat64
#  define UTIL_fstat _fstat64
#  define UTIL_STAT_MODE_ISREG(st_mode) ((st_mode) & S_IFREG)
#elif   defined(__MINGW32__) && defined (__MSVCRT__)
#  define UTIL_TYPE_stat _stati64
#  define UTIL_stat _stati64
#  define UTIL_fstat _fstati64
#  define UTIL_STAT_MODE_ISREG(st_mode) ((st_mode) & S_IFREG)
#else
#  define UTIL_TYPE_stat stat
#  define UTIL_stat stat
#  define UTIL_fstat fstat
#  define UTIL_STAT_MODE_ISREG(st_mode) (S_ISREG(st_mode))
#endif


/*-****************************************
*  fileno() function
******************************************/
#if defined(_MSC_VER)
#  define UTIL_fileno _fileno
#else
#  define UTIL_fileno fileno
#endif

/* *************************************
*  Constants
***************************************/
#define LIST_SIZE_INCREASE   (8*1024)


/*-****************************************
*  Compiler specifics
******************************************/
#if defined(__INTEL_COMPILER)
#  pragma warning(disable : 177)    /* disable: message #177: function was declared but never referenced, useful with UTIL_STATIC */
#endif
#if defined(__GNUC__)
#  define UTIL_STATIC static __attribute__((unused))
#elif defined (__cplusplus) || (defined (__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) /* C99 */)
#  define UTIL_STATIC static inline
#elif defined(_MSC_VER)
#  define UTIL_STATIC static __inline
#else
#  define UTIL_STATIC static  /* this version may generate warnings for unused static functions; disable the relevant warning */
#endif



/*-****************************************
*  Allocation functions
******************************************/
/*
 * A modified version of realloc().
 * Original block is always freed.
 */
UTIL_STATIC void* UTIL_realloc(void* prevPtr, size_t newSize)
{
    if (newSize == 0) {
        free(prevPtr);
        return NULL;
    }
    /* newSize > 0 */
    {   void* const newptr = realloc(prevPtr, newSize);
        if (newptr) return newptr;
    }
    /* realloc failed */
    free(prevPtr);
    return NULL;
}


/*-****************************************
*  String functions
******************************************/
/* supports a==NULL or b==NULL */
UTIL_STATIC int UTIL_sameString(const char* a, const char* b)
{
    assert(a != NULL || b != NULL);  /* unsupported scenario */
    if (a==NULL) return 0;
    if (b==NULL) return 0;
    return !strcmp(a,b);
}



/*-****************************************
*  File functions
******************************************/
#if defined(_MSC_VER)
    #define chmod _chmod
    typedef struct __stat64 stat_t;
#else
    typedef struct stat stat_t;
#endif


UTIL_STATIC int UTIL_isRegFile(const char* infilename);
UTIL_STATIC int UTIL_isRegFD(int fd);


UTIL_STATIC int UTIL_setFileStat(const char *filename, stat_t *statbuf)
{
    int res = 0;

    if (!UTIL_isRegFile(filename))
        return -1;

    {
#if defined(_WIN32) || (PLATFORM_POSIX_VERSION < 200809L)
        struct utimbuf timebuf;
        timebuf.actime = time(NULL);
        timebuf.modtime = statbuf->st_mtime;
        res += utime(filename, &timebuf);  /* set access and modification times */
#else
        struct timespec timebuf[2];
        memset(timebuf, 0, sizeof(timebuf));
        timebuf[0].tv_nsec = UTIME_NOW;
        timebuf[1].tv_sec = statbuf->st_mtime;
        res += utimensat(AT_FDCWD, filename, timebuf, 0);  /* set access and modification times */
#endif
    }

#if !defined(_WIN32)
    res += chown(filename, statbuf->st_uid, statbuf->st_gid);  /* Copy ownership */
#endif

    res += chmod(filename, statbuf->st_mode & 07777);  /* Copy file permissions */

    errno = 0;
    return -res; /* number of errors is returned */
}


UTIL_STATIC int UTIL_getFDStat(int fd, stat_t *statbuf)
{
    int r;
#if defined(_MSC_VER)
    r = _fstat64(fd, statbuf);
    if (r || !(statbuf->st_mode & S_IFREG)) return 0;   /* No good... */
#else
    r = fstat(fd, statbuf);
    if (r || !S_ISREG(statbuf->st_mode)) return 0;   /* No good... */
#endif
    return 1;
}

UTIL_STATIC int UTIL_getFileStat(const char* infilename, stat_t *statbuf)
{
    int r;
#if defined(_MSC_VER)
    r = _stat64(infilename, statbuf);
    if (r || !(statbuf->st_mode & S_IFREG)) return 0;   /* No good... */
#else
    r = stat(infilename, statbuf);
    if (r || !S_ISREG(statbuf->st_mode)) return 0;   /* No good... */
#endif
    return 1;
}

UTIL_STATIC int UTIL_isRegFD(int fd)
{
    stat_t statbuf;
#ifdef _WIN32
    /* Windows runtime library always open file descriptors 0, 1 and 2 in text mode, therefore we can't use them for binary I/O */
    if(fd < 3) return 0;
#endif
    return UTIL_getFDStat(fd, &statbuf); /* Only need to know whether it is a regular file */
}

UTIL_STATIC int UTIL_isRegFile(const char* infilename)
{
    stat_t statbuf;
    return UTIL_getFileStat(infilename, &statbuf); /* Only need to know whether it is a regular file */
}

UTIL_STATIC int UTIL_isDirectory(const char* infilename)
{
    stat_t statbuf;
    int r;
#if defined(_MSC_VER)
    r = _stat64(infilename, &statbuf);
    if (r) return 0;
    return (statbuf.st_mode & S_IFDIR);
#else
    r = stat(infilename, &statbuf);
    if (r) return 0;
    return (S_ISDIR(statbuf.st_mode));
#endif
}


UTIL_STATIC U64 UTIL_getOpenFileSize(FILE* file)
{
    int r;
    int fd;
    struct UTIL_TYPE_stat statbuf;

    fd = UTIL_fileno(file);
    if (fd < 0) {
        perror("fileno");
        exit(1);
    }
    r = UTIL_fstat(fd, &statbuf);
    if (r || !UTIL_STAT_MODE_ISREG(statbuf.st_mode)) return 0;   /* No good... */
    return (U64)statbuf.st_size;
}


UTIL_STATIC U64 UTIL_getFileSize(const char* infilename)
{
    int r;
    struct UTIL_TYPE_stat statbuf;

    r = UTIL_stat(infilename, &statbuf);
    if (r || !UTIL_STAT_MODE_ISREG(statbuf.st_mode)) return 0;   /* No good... */
    return (U64)statbuf.st_size;
}


UTIL_STATIC U64 UTIL_getTotalFileSize(const char** fileNamesTable, unsigned nbFiles)
{
    U64 total = 0;
    unsigned n;
    for (n=0; n<nbFiles; n++)
        total += UTIL_getFileSize(fileNamesTable[n]);
    return total;
}


#ifdef _WIN32
#  define UTIL_HAS_CREATEFILELIST

UTIL_STATIC int UTIL_prepareFileList(const char* dirName, char** bufStart, size_t* pos, char** bufEnd)
{
    char* path;
    size_t dirLength, nbFiles = 0;
    WIN32_FIND_DATAA cFile;
    HANDLE hFile;

    dirLength = strlen(dirName);
    path = (char*) malloc(dirLength + 3);
    if (!path) return 0;

    memcpy(path, dirName, dirLength);
    path[dirLength] = '\\';
    path[dirLength+1] = '*';
    path[dirLength+2] = 0;

    hFile=FindFirstFileA(path, &cFile);
    if (hFile == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Cannot open directory '%s'\n", dirName);
        return 0;
    }
    free(path);

    do {
        size_t pathLength;
        int const fnameLength = (int)strlen(cFile.cFileName);
        path = (char*) malloc(dirLength + fnameLength + 2);
        if (!path) { FindClose(hFile); return 0; }
        memcpy(path, dirName, dirLength);
        path[dirLength] = '\\';
        memcpy(path+dirLength+1, cFile.cFileName, fnameLength);
        pathLength = dirLength+1+fnameLength;
        path[pathLength] = 0;
        if (cFile.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (strcmp (cFile.cFileName, "..") == 0 ||
                strcmp (cFile.cFileName, ".") == 0) continue;

            nbFiles += UTIL_prepareFileList(path, bufStart, pos, bufEnd);  /* Recursively call "UTIL_prepareFileList" with the new path. */
            if (*bufStart == NULL) { free(path); FindClose(hFile); return 0; }
        }
        else if ((cFile.dwFileAttributes & FILE_ATTRIBUTE_NORMAL) || (cFile.dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE) || (cFile.dwFileAttributes & FILE_ATTRIBUTE_COMPRESSED)) {
            if (*bufStart + *pos + pathLength >= *bufEnd) {
                ptrdiff_t newListSize = (*bufEnd - *bufStart) + LIST_SIZE_INCREASE;
                *bufStart = (char*)UTIL_realloc(*bufStart, newListSize);
                *bufEnd = *bufStart + newListSize;
                if (*bufStart == NULL) { free(path); FindClose(hFile); return 0; }
            }
            if (*bufStart + *pos + pathLength < *bufEnd) {
                strncpy(*bufStart + *pos, path, *bufEnd - (*bufStart + *pos));
                *pos += pathLength + 1;
                nbFiles++;
            }
        }
        free(path);
    } while (FindNextFileA(hFile, &cFile));

    FindClose(hFile);
    assert(nbFiles < INT_MAX);
    return (int)nbFiles;
}

#elif defined(__linux__) || (PLATFORM_POSIX_VERSION >= 200112L)  /* opendir, readdir require POSIX.1-2001 */
#  define UTIL_HAS_CREATEFILELIST
#  include <dirent.h>       /* opendir, readdir */
#  include <string.h>       /* strerror, memcpy */

UTIL_STATIC int UTIL_prepareFileList(const char* dirName, char** bufStart, size_t* pos, char** bufEnd)
{
    DIR* dir;
    struct dirent * entry;
    size_t dirLength;
    int nbFiles = 0;

    if (!(dir = opendir(dirName))) {
        fprintf(stderr, "Cannot open directory '%s': %s\n", dirName, strerror(errno));
        return 0;
    }

    dirLength = strlen(dirName);
    errno = 0;
    while ((entry = readdir(dir)) != NULL) {
        char* path;
        size_t fnameLength, pathLength;
        if (strcmp (entry->d_name, "..") == 0 ||
            strcmp (entry->d_name, ".") == 0) continue;
        fnameLength = strlen(entry->d_name);
        path = (char*)malloc(dirLength + fnameLength + 2);
        if (!path) { closedir(dir); return 0; }
        memcpy(path, dirName, dirLength);
        path[dirLength] = '/';
        memcpy(path+dirLength+1, entry->d_name, fnameLength);
        pathLength = dirLength+1+fnameLength;
        path[pathLength] = 0;

        if (UTIL_isDirectory(path)) {
            nbFiles += UTIL_prepareFileList(path, bufStart, pos, bufEnd);  /* Recursively call "UTIL_prepareFileList" with the new path. */
            if (*bufStart == NULL) { free(path); closedir(dir); return 0; }
        } else {
            if (*bufStart + *pos + pathLength >= *bufEnd) {
                size_t const newListSize = (size_t)(*bufEnd - *bufStart) + LIST_SIZE_INCREASE;
                *bufStart = (char*)UTIL_realloc(*bufStart, newListSize);
                *bufEnd = *bufStart + newListSize;
                if (*bufStart == NULL) { free(path); closedir(dir); return 0; }
            }
            if (*bufStart + *pos + pathLength < *bufEnd) {
                strncpy(*bufStart + *pos, path, *bufEnd - (*bufStart + *pos));
                *pos += pathLength + 1;
                nbFiles++;
            }
        }
        free(path);
        errno = 0; /* clear errno after UTIL_isDirectory, UTIL_prepareFileList */
    }

    if (errno != 0) {
        fprintf(stderr, "readdir(%s) error: %s\n", dirName, strerror(errno));
        free(*bufStart);
        *bufStart = NULL;
    }
    closedir(dir);
    return nbFiles;
}

#else

UTIL_STATIC int UTIL_prepareFileList(const char* dirName, char** bufStart, size_t* pos, char** bufEnd)
{
    (void)bufStart; (void)bufEnd; (void)pos;
    fprintf(stderr, "Directory %s ignored (compiled without _WIN32 or _POSIX_C_SOURCE)\n", dirName);
    return 0;
}

#endif /* #ifdef _WIN32 */

/*
 * UTIL_createFileList - takes a list of files and directories (params: inputNames, inputNamesNb), scans directories,
 *                       and returns a new list of files (params: return value, allocatedBuffer, allocatedNamesNb).
 * After finishing usage of the list the structures should be freed with UTIL_freeFileList(params: return value, allocatedBuffer)
 * In case of error UTIL_createFileList returns NULL and UTIL_freeFileList should not be called.
 */
UTIL_STATIC const char**
UTIL_createFileList(const char** inputNames, unsigned inputNamesNb,
                    char** allocatedBuffer, unsigned* allocatedNamesNb)
{
    size_t pos;
    unsigned i, nbFiles;
    char* buf = (char*)malloc(LIST_SIZE_INCREASE);
    size_t bufSize = LIST_SIZE_INCREASE;
    const char** fileTable;

    if (!buf) return NULL;

    for (i=0, pos=0, nbFiles=0; i<inputNamesNb; i++) {
        if (!UTIL_isDirectory(inputNames[i])) {
            size_t const len = strlen(inputNames[i]) + 1;  /* include nul char */
            if (pos + len >= bufSize) {
                while (pos + len >= bufSize) bufSize += LIST_SIZE_INCREASE;
                buf = (char*)UTIL_realloc(buf, bufSize);
                if (!buf) return NULL;
            }
            assert(pos + len < bufSize);
            memcpy(buf + pos, inputNames[i], len);
            pos += len;
            nbFiles++;
        } else {
            char* bufend = buf + bufSize;
            nbFiles += (unsigned)UTIL_prepareFileList(inputNames[i], &buf, &pos, &bufend);
            if (buf == NULL) return NULL;
            assert(bufend > buf);
            bufSize = (size_t)(bufend - buf);
    }   }

    if (nbFiles == 0) { free(buf); return NULL; }

    fileTable = (const char**)malloc(((size_t)nbFiles+1) * sizeof(const char*));
    if (!fileTable) { free(buf); return NULL; }

    for (i=0, pos=0; i<nbFiles; i++) {
        fileTable[i] = buf + pos;
        pos += strlen(fileTable[i]) + 1;
    }

    if (pos > bufSize) {
        free(buf);
        free((void*)fileTable);
        return NULL;
    }   /* can this happen ? */

    *allocatedBuffer = buf;
    *allocatedNamesNb = nbFiles;

    return fileTable;
}


UTIL_STATIC void
UTIL_freeFileList(const char** filenameTable, char* allocatedBuffer)
{
    free(allocatedBuffer);
    free((void*)filenameTable);
}


#if defined (__cplusplus)
}
#endif

#endif /* UTIL_H_MODULE */
