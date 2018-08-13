/*
 * File: condvar3_2.c
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
 * Test Synopsis:
 * - Test timeout of multiple waits on a CV with remainder broadcast awoken.
 *
 * Test Method (Validation or Falsification):
 * - Validation
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
 * - Because some CVs are never signaled, we expect their waits to time out.
 *   Some time out, the rest are broadcast signaled. Pthread_cond_destroy() will fail
 *   unless all are accounted for, either signaled or timedout.
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
 * - 
 *
 * Pass Criteria:
 * - pthread_cond_timedwait returns ETIMEDOUT.
 * - Process returns zero exit status.
 *
 * Fail Criteria:
 * - pthread_cond_timedwait does not return ETIMEDOUT.
 * - Process returns non-zero exit status.
 */

#define _WIN32_WINNT 0x400

#include "test.h"
#include <sys/timeb.h>

static pthread_cond_t cv;
static pthread_mutex_t mutex;
static struct timespec abstime, abstime2;
static struct timespec reltime = { 5, 0 };
static int timedout = 0;
static int awoken = 0;

enum {
  NUMTHREADS = 30
};

void *
mythread(void * arg)
{
  int result;

  assert(pthread_mutex_lock(&mutex) == 0);

  abstime2.tv_sec = abstime.tv_sec;

  if ((int) (size_t)arg % 3 == 0)
    {
      abstime2.tv_sec += 2;
    }

  result = pthread_cond_timedwait(&cv, &mutex, &abstime2);
  assert(pthread_mutex_unlock(&mutex) == 0);
  if (result == ETIMEDOUT)
    {
      InterlockedIncrement((LPLONG)&timedout);
    }
  else
    {
      InterlockedIncrement((LPLONG)&awoken);
    }

  return arg;
}

/* Cheating here - sneaking a peek at library internals */
#include "../config.h"
#include "../implement.h"

int
main()
{
  int i;
  pthread_t t[NUMTHREADS + 1];
  void* result = (void*)0;

  assert(pthread_cond_init(&cv, NULL) == 0);

  assert(pthread_mutex_init(&mutex, NULL) == 0);

  (void) pthread_win32_getabstime_np(&abstime, &reltime);
  abstime2.tv_sec = abstime.tv_sec;
  abstime2.tv_nsec = abstime.tv_nsec;

  assert(pthread_mutex_lock(&mutex) == 0);

  for (i = 1; i <= NUMTHREADS; i++)
    {
      assert(pthread_create(&t[i], NULL, mythread, (void *)(size_t)i) == 0);
    }

  assert(pthread_mutex_unlock(&mutex) == 0);

  for (i = 1; i <= NUMTHREADS; i++)
    {
      assert(pthread_join(t[i], &result) == 0);
	assert((int)(size_t)result == i);
      /*
       * Approximately 2/3rds of the threads are expected to time out.
       * Signal the remainder after some threads have woken up and exited
       * and while some are still waking up after timeout.
       * Also tests that redundant broadcasts don't return errors.
       */

//      assert(pthread_mutex_lock(&mutex) == 0);

      if (InterlockedExchangeAdd((LPLONG)&awoken, 0L) > NUMTHREADS/3)
        {
          assert(pthread_cond_broadcast(&cv) == 0);
        }

//      assert(pthread_mutex_unlock(&mutex) == 0);

    }

  assert(awoken == NUMTHREADS - timedout);

  {
  int result = pthread_cond_destroy(&cv);
  if (result != 0)
    {
      fprintf(stderr, "Result = %s\n", error_string[result]);
	fprintf(stderr, "\tWaitersBlocked = %ld\n", cv->nWaitersBlocked);
	fprintf(stderr, "\tWaitersGone = %ld\n", cv->nWaitersGone);
	fprintf(stderr, "\tWaitersToUnblock = %ld\n", cv->nWaitersToUnblock);
	fflush(stderr);
    }
  assert(result == 0);
  }

  assert(pthread_mutex_destroy(&mutex) == 0);

  return 0;
}
