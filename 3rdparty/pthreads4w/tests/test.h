/*
 * test.h
 *
 * Useful definitions and declarations for tests.
 *
 *
 * --------------------------------------------------------------------------
 *
 *      Pthreads4w - POSIX Threads Library for Win32
 *      Copyright(C) 1998 John E. Bossom
 *      Copyright(C) 1999-2018, Pthreads4w contributors
 *
 *      Homepage: https://sourceforge.net/projects/pthreads4w/
 *
 *      The current list of contributors is contained
 *      in the file CONTRIBUTORS included with the source
 *      code distribution. The list can also be seen at the
 *      following World Wide Web location:
 *      https://sourceforge.net/p/pthreads4w/wiki/Contributors/
 *
 * This file is part of Pthreads4w.
 *
 *    Pthreads4w is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    Pthreads4w is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with Pthreads4w.  If not, see <http://www.gnu.org/licenses/>. *
 *
 */

#ifndef _PTHREAD_TEST_H_
#define _PTHREAD_TEST_H_

/*
 * Some tests sneak a peek at ../implement.h
 * This is used inside ../implement.h to control
 * what these test apps see and don't see.
 */
#define PTW32_TEST_SNEAK_PEEK

#include "pthread.h"
#include "sched.h"
#include "semaphore.h"

#include <windows.h>
#include <stdio.h>
#include <sys/timeb.h>
/*
 * FIXME: May not be available on all platforms.
 */
#include <errno.h>

#define PTW32_THREAD_NULL_ID {NULL,0}

/*
 * Some non-thread POSIX API substitutes
 */
#if !defined(__MINGW64_VERSION_MAJOR)
#  define rand_r( _seed ) \
        ( _seed == _seed? rand() : rand() )
#endif

#if defined(__MINGW32__)
# include <stdint.h>
#elif defined(__BORLANDC__)
# define int64_t ULONGLONG
#else
# define int64_t _int64
#endif

#if defined(_MSC_VER) && _MSC_VER >= 1400
#  define PTW32_FTIME(x) _ftime64_s(x)
#  define PTW32_STRUCT_TIMEB struct __timeb64
#elif ( defined(_MSC_VER) && _MSC_VER >= 1300 ) || \
      ( defined(__MINGW32__) && __MSVCRT_VERSION__ >= 0x0601 )
#  define PTW32_FTIME(x) _ftime64(x)
#  define PTW32_STRUCT_TIMEB struct __timeb64
#else
#  define PTW32_FTIME(x) _ftime(x)
#  define PTW32_STRUCT_TIMEB struct _timeb
#endif


const char * error_string[] = {
  "ZERO_or_EOK",
  "EPERM",
  "ENOFILE_or_ENOENT",
  "ESRCH",
  "EINTR",
  "EIO",
  "ENXIO",
  "E2BIG",
  "ENOEXEC",
  "EBADF",
  "ECHILD",
  "EAGAIN",
  "ENOMEM",
  "EACCES",
  "EFAULT",
  "UNKNOWN_15",
  "EBUSY",
  "EEXIST",
  "EXDEV",
  "ENODEV",
  "ENOTDIR",
  "EISDIR",
  "EINVAL",
  "ENFILE",
  "EMFILE",
  "ENOTTY",
  "UNKNOWN_26",
  "EFBIG",
  "ENOSPC",
  "ESPIPE",
  "EROFS",
  "EMLINK",
  "EPIPE",
  "EDOM",
  "ERANGE",
  "UNKNOWN_35",
  "EDEADLOCK_or_EDEADLK",
  "UNKNOWN_37",
  "ENAMETOOLONG",
  "ENOLCK",
  "ENOSYS",
  "ENOTEMPTY",
#if PTW32_VERSION_MAJOR > 2
  "EILSEQ",
#else
  "EILSEQ_or_EOWNERDEAD",
  "ENOTRECOVERABLE"
#endif
};

/*
 * The Mingw32 assert macro calls the CRTDLL _assert function
 * which pops up a dialog. We want to run in batch mode so
 * we define our own assert macro.
 */
#ifdef assert
# undef assert
#endif

#ifndef ASSERT_TRACE
# define ASSERT_TRACE 0
#else
# undef ASSERT_TRACE
# define ASSERT_TRACE 1
#endif

# define assert(e) \
   ((e) ? ((ASSERT_TRACE) ? fprintf(stderr, \
                                    "Assertion succeeded: (%s), file %s, line %d\n", \
			            #e, __FILE__, (int) __LINE__), \
	                            fflush(stderr) : \
                             0) : \
          (fprintf(stderr, "Assertion failed: (%s), file %s, line %d\n", \
                   #e, __FILE__, (int) __LINE__), exit(1), 0))

int assertE;
# define assert_e(e, o, r) \
   (((assertE = e) o (r)) ? ((ASSERT_TRACE) ? fprintf(stderr, \
                                    "Assertion succeeded: (%s), file %s, line %d\n", \
			            #e, __FILE__, (int) __LINE__), \
	                            fflush(stderr) : \
                             0) : \
       (assertE <= (int) (sizeof(error_string)/sizeof(error_string[0]))) ? \
	   (fprintf(stderr, "Assertion failed: (%s %s %s), file %s, line %d, error %s\n", \
			    #e,#o,#r, __FILE__, (int) __LINE__, error_string[assertE]), exit(1), 0) :\
		   (fprintf(stderr, \
			    "Assertion failed: (%s %s %s), file %s, line %d, error %d\n", \
		            #e,#o,#r, __FILE__, (int) __LINE__, assertE), exit(1), 0))

#endif

# define BEGIN_MUTEX_STALLED_ROBUST(mxAttr) \
  for(;;) \
    { \
      static int _i=0; \
      static int _robust; \
      pthread_mutexattr_getrobust(&(mxAttr), &_robust);

# define END_MUTEX_STALLED_ROBUST(mxAttr) \
      printf("Pass %s\n", _robust==PTHREAD_MUTEX_ROBUST?"Robust":"Non-robust"); \
      if (++_i > 1) \
        break; \
      else \
        { \
          pthread_mutexattr_t *pma, *pmaEnd; \
          for(pma = &(mxAttr), pmaEnd = pma + sizeof(mxAttr)/sizeof(pthread_mutexattr_t); \
              pma < pmaEnd; \
              pthread_mutexattr_setrobust(pma++, PTHREAD_MUTEX_ROBUST)); \
        } \
    }

# define IS_ROBUST (_robust==PTHREAD_MUTEX_ROBUST)
