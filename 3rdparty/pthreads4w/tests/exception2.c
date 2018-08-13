/*
 * File: exception2.c
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
 * --------------------------------------------------------------------------
 *
 * Test Synopsis: Test passing of exceptions out of thread scope.
 *
 * Test Method (Validation or Falsification):
 * - 
 *
 * Requirements Tested:
 * -
 *
 * Features Tested:
 * - 
 *
 * Cases Tested:
 * - 
 *
 * Description:
 * - 
 *
 * Environment:
 * - 
 *
 * Input:
 * - None.
 *
 * Output:
 * - File name, Line number, and failed expression on failure.
 * - No output on success.
 *
 * Assumptions:
 * - have working pthread_create, pthread_self, pthread_mutex_lock/unlock
 *   pthread_testcancel, pthread_cancel
 *
 * Pass Criteria:
 * - Process returns zero exit status.
 *
 * Fail Criteria:
 * - Process returns non-zero exit status.
 */


#if defined(_MSC_VER) || defined(__cplusplus)

#if defined(_MSC_VER) && defined(__cplusplus)
#include <eh.h>
#elif defined(__cplusplus)
#include <exception>
#endif

#ifdef __GNUC__
#include <stdlib.h>
#endif

#include "test.h"

/*
 * Create NUMTHREADS threads in addition to the Main thread.
 */
enum {
  NUMTHREADS = 1
};


void *
exceptionedThread(void * arg)
{
  int dummy = 0x1;

#if defined(_MSC_VER) && !defined(__cplusplus)

  RaiseException(dummy, 0, 0, NULL);

#elif defined(__cplusplus)

  throw dummy;

#endif

  return (void *) 100;
}

int
main(int argc, char* argv[])
{
  int i;
  pthread_t mt;
  pthread_t et[NUMTHREADS];

  DWORD dwMode = SetErrorMode(SEM_NOGPFAULTERRORBOX);
  SetErrorMode(dwMode | SEM_NOGPFAULTERRORBOX);

  if (argc <= 1)
    {
      int result;

      printf("You should see an \"abnormal termination\" message\n");
      fflush(stdout);

      result = system("exception2.exe die");

      printf("\"exception2.exe die\" returned status %d\n", result);

      /*
       * result should be 0, 1 or 3 depending on build settings
       */
      exit((result == 0 || result == 1 || result == 3) ? 0 : 1);
    }

#if defined(NO_ERROR_DIALOGS)
  SetErrorMode(SEM_NOGPFAULTERRORBOX);
#endif

  assert((mt = pthread_self()).p != NULL);

  for (i = 0; i < NUMTHREADS; i++)
    {
      assert(pthread_create(&et[i], NULL, exceptionedThread, NULL) == 0);
    }

  Sleep(100);

  /*
   * Success.
   */
  return 0;
}

#else /* defined(_MSC_VER) || defined(__cplusplus) */

#include <stdio.h>

int
main()
{
  fprintf(stderr, "Test N/A for this compiler environment.\n");
  return 0;
}

#endif /* defined(_MSC_VER) || defined(__cplusplus) */
