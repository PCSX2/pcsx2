/*
 * File: context2.c
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
 * Test Synopsis: Test context switching method.
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
 * - pthread_create
 *   pthread_exit
 *
 * Pass Criteria:
 * - Process returns zero exit status.
 *
 * Fail Criteria:
 * - Process returns non-zero exit status.
 */

#define _WIN32_WINNT 0x400

#include "test.h"
/* Cheating here - sneaking a peek at library internals */
#include "../config.h"
#include "../implement.h"
#include "../context.h"

static int washere = 0;
static volatile size_t tree_counter = 0;

#ifdef _MSC_VER
# pragma inline_depth(0)
# pragma optimize("g", off)
#endif

static size_t tree(size_t depth)
{
  if (! depth--)
    return tree_counter++;

  return tree(depth) + tree(depth);
}

static void * func(void * arg)
{
  washere = 1;

  return (void *) tree(64);
}

static void
anotherEnding ()
{
  /*
   * Switched context
   */
  washere++;
  pthread_exit(0);
}

#ifdef _MSC_VER
# pragma inline_depth()
# pragma optimize("", on)
#endif

int
main()
{
  pthread_t t;
  HANDLE hThread;

  assert(pthread_create(&t, NULL, func, NULL) == 0);

  hThread = ((ptw32_thread_t *)t.p)->threadH;

  Sleep(500);

  SuspendThread(hThread);

  if (WaitForSingleObject(hThread, 0) == WAIT_TIMEOUT)
    {
      /*
       * Ok, thread did not exit before we got to it.
       */
      CONTEXT context;

      context.ContextFlags = CONTEXT_CONTROL;

      GetThreadContext(hThread, &context);
      PTW32_PROGCTR (context) = (DWORD_PTR) anotherEnding;
      SetThreadContext(hThread, &context);
      ResumeThread(hThread);
    }
  else
    {
      printf("Exited early\n");
      fflush(stdout);
    }

  Sleep(1000);

  assert(washere == 2);

  return 0;
}
