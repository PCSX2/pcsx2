/*
 * File: sequence1.c
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
 * - that unique thread sequence numbers are generated.
 * - Analyse thread struct reuse.
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
 * - This test is implementation specific
 * because it uses knowledge of internals that should be
 * opaque to an application.
 *
 * Input:
 * - None.
 *
 * Output:
 * - File name, Line number, and failed expression on failure.
 * - analysis output on success.
 *
 * Assumptions:
 * -
 *
 * Pass Criteria:
 * - unique sequence numbers are generated for every new thread.
 *
 * Fail Criteria:
 * - 
 */

#include "test.h"

/*
 */

enum {
	NUMTHREADS = PTHREAD_THREADS_MAX
};


static long done = 0;
/*
 * seqmap should have 1 in every element except [0]
 * Thread sequence numbers start at 1 and we will also
 * include this main thread so we need NUMTHREADS+2
 * elements. 
 */
static UINT64 seqmap[NUMTHREADS+2];

void * func(void * arg)
{
  sched_yield();
  seqmap[(int)pthread_getunique_np(pthread_self())] = 1;
  InterlockedIncrement(&done);

  return (void *) 0; 
}
 
int
main()
{
  pthread_t t[NUMTHREADS];
  pthread_attr_t attr;
  int i;

  assert(pthread_attr_init(&attr) == 0);
  assert(pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) == 0);

  for (i = 0; i < NUMTHREADS+2; i++)
    {
      seqmap[i] = 0;
    }

  for (i = 0; i < NUMTHREADS; i++)
    {
      if (NUMTHREADS/2 == i)
        {
          /* Include this main thread, which will be an implicit pthread_t */
          seqmap[(int)pthread_getunique_np(pthread_self())] = 1;
        }
      assert(pthread_create(&t[i], &attr, func, NULL) == 0);
    }

  while (NUMTHREADS > InterlockedExchangeAdd((LPLONG)&done, 0L))
    Sleep(100);

  Sleep(100);

  assert(seqmap[0] == 0);
  for (i = 1; i < NUMTHREADS+2; i++)
    {
      assert(seqmap[i] == 1);
    }

  return 0;
}
