/*
 * File: exception3.c
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
 * Test Synopsis: Test running of user supplied terminate() function.
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

#include "test.h"

/*
 * Note: Due to a buggy C++ runtime in Visual Studio 2005, when we are
 * built with /MD and an unhandled exception occurs, the runtime does not
 * properly call the terminate handler specified by set_terminate().
 */
#if defined(__cplusplus) \
	&& !(defined(_MSC_VER) && _MSC_VER == 1400 && defined(_DLL) && !defined(_DEBUG))

#if defined(_MSC_VER)
# include <eh.h>
#else
# if defined(__GNUC__) && __GNUC__ < 3
#   include <new.h>
# else
#   include <new>
    using std::set_terminate;
# endif
#endif

/*
 * Create NUMTHREADS threads in addition to the Main thread.
 */
enum {
  NUMTHREADS = 10
};

int caught = 0;
pthread_mutex_t caughtLock;

void
terminateFunction ()
{
  assert(pthread_mutex_lock(&caughtLock) == 0);
  caught++;
#if 0
  {
     FILE * fp = fopen("pthread.log", "a");
     fprintf(fp, "Caught = %d\n", caught);
     fclose(fp);
  }
#endif
  assert_e(pthread_mutex_unlock(&caughtLock), ==, 0);

  /*
   * Notes from the MSVC++ manual:
   *       1) A term_func() should call exit(), otherwise
   *          abort() will be called on return to the caller.
   *          abort() raises SIGABRT. The default signal handler
   *          for all signals terminates the calling program with
   *          exit code 3.
   *       2) A term_func() must not throw an exception. Dev: Therefore
   *          term_func() should not call pthread_exit() if an
   *          exception-using version of Pthreads4w library
   *          is being used (i.e. either pthreadVCE or pthreadVSE).
   */
  /*
   * Allow time for all threads to reach here before exit, otherwise
   * threads will be terminated while holding the lock and cause
   * the next unlock to return EPERM (we're using ERRORCHECK mutexes).
   * Perhaps this would be a good test for robust mutexes.
   */
  Sleep(20);

  exit(0);
}

void
wrongTerminateFunction ()
{
  fputs("This is not the termination routine that should have been called!\n", stderr);
  exit(1);
}

void *
exceptionedThread(void * arg)
{
  int dummy = 0x1;

#if defined(PTW32_USES_SEPARATE_CRT) && (defined(__CLEANUP_CXX) || defined(__CLEANUP_SEH))
  printf("PTW32_USES_SEPARATE_CRT is defined\n");
  pthread_win32_set_terminate_np(&terminateFunction);
  set_terminate(&wrongTerminateFunction);
#else
  set_terminate(&terminateFunction);
#endif

  throw dummy;

  return (void *) 0;
}

int
main()
{
  int i;
  pthread_t mt;
  pthread_t et[NUMTHREADS];
  pthread_mutexattr_t ma;

  DWORD dwMode = SetErrorMode(SEM_NOGPFAULTERRORBOX);
  SetErrorMode(dwMode | SEM_NOGPFAULTERRORBOX);

  assert((mt = pthread_self()).p != NULL);

  printf("See the notes inside of exception3.c re term_funcs.\n");

  assert(pthread_mutexattr_init(&ma) == 0);
  assert(pthread_mutexattr_settype(&ma, PTHREAD_MUTEX_ERRORCHECK) == 0);
  assert(pthread_mutex_init(&caughtLock, &ma) == 0);
  assert(pthread_mutexattr_destroy(&ma) == 0);

  for (i = 0; i < NUMTHREADS; i++)
    {
      assert(pthread_create(&et[i], NULL, exceptionedThread, NULL) == 0);
    }

  while (true);

  /*
   * Should never be reached.
   */
  return 1;
}

#else /* defined(__cplusplus) */

#include <stdio.h>

int
main()
{
  fprintf(stderr, "Test N/A for this compiler environment.\n");
  return 0;
}

#endif /* defined(__cplusplus) */
