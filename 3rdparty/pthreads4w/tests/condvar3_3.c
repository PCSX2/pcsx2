/*
 * File: condvar3_3.c
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
 * - Test timeouts and lost signals on a CV.
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

/* Timur Aydin (taydin@snet.net) */

#include "test.h"

#include <sys/timeb.h>

pthread_cond_t cnd;
pthread_mutex_t mtx;

static const long NANOSEC_PER_SEC = 1000000000L;

int main()
{
   int rc;
   struct timespec abstime, reltime = { 0, NANOSEC_PER_SEC/2 };

   assert(pthread_cond_init(&cnd, 0) == 0);
   assert(pthread_mutex_init(&mtx, 0) == 0);

   (void) pthread_win32_getabstime_np(&abstime, &reltime);

   /* Here pthread_cond_timedwait should time out after one second. */

   assert(pthread_mutex_lock(&mtx) == 0);

   assert((rc = pthread_cond_timedwait(&cnd, &mtx, &abstime)) == ETIMEDOUT);

   assert(pthread_mutex_unlock(&mtx) == 0);

   /* Here, the condition variable is signalled, but there are no
      threads waiting on it. The signal should be lost and
      the next pthread_cond_timedwait should time out too. */

   assert((rc = pthread_cond_signal(&cnd)) == 0);

   assert(pthread_mutex_lock(&mtx) == 0);

   (void) pthread_win32_getabstime_np(&abstime, &reltime);

   assert((rc = pthread_cond_timedwait(&cnd, &mtx, &abstime)) == ETIMEDOUT);

   assert(pthread_mutex_unlock(&mtx) == 0);

   return 0;
}
