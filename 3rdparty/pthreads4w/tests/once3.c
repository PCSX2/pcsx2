/*
 * once3.c
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
 * Create several pthread_once objects and channel several threads
 * through each. Make the init_routine cancelable and cancel them with
 * waiters waiting.
 *
 * Depends on API functions:
 *	pthread_once()
 *	pthread_create()
 *      pthread_testcancel()
 *      pthread_cancel()
 *      pthread_once()
 */

/* #define ASSERT_TRACE */

#include "test.h"

#define NUM_THREADS 100 /* Targeting each once control */
#define NUM_ONCE    10

static pthread_once_t o = PTHREAD_ONCE_INIT;
static pthread_once_t once[NUM_ONCE];

typedef struct {
  int i;
  CRITICAL_SECTION cs;
} sharedInt_t;

static sharedInt_t numOnce;
static sharedInt_t numThreads;

void
myfunc(void)
{
  EnterCriticalSection(&numOnce.cs);
  numOnce.i++;
  assert(numOnce.i > 0);
  LeaveCriticalSection(&numOnce.cs);
  /* Simulate slow once routine so that following threads pile up behind it */
  Sleep(10);
  /* Test for cancellation late so we're sure to have waiters. */
  pthread_testcancel();
}

void *
mythread(void * arg)
{
  /*
   * Cancel every thread. These threads are deferred cancelable only, so
   * this thread will see it only when it performs the once routine (my_func).
   * The result will be that every thread eventually cancels only when it
   * becomes the new 'once' thread.
   */
  assert(pthread_cancel(pthread_self()) == 0);
  /*
   * Now we block on the 'once' control.
   */
  assert(pthread_once(&once[(int)(size_t)arg], myfunc) == 0);
  /*
   * We should never get to here.
   */
  EnterCriticalSection(&numThreads.cs);
  numThreads.i++;
  LeaveCriticalSection(&numThreads.cs);
  return (void*)(size_t)0;
}

int
main()
{
  pthread_t t[NUM_THREADS][NUM_ONCE];
  int i, j;

#if defined(PTW32_CONFIG_MSVC6) && defined(__CLEANUP_CXX)
  puts("If this test fails or hangs, rebuild the library with /EHa instead of /EHs.");
  puts("(This is a known issue with Microsoft VC++6.0.)");
  fflush(stdout);
#endif
  
  memset(&numOnce, 0, sizeof(sharedInt_t));
  memset(&numThreads, 0, sizeof(sharedInt_t));

  InitializeCriticalSection(&numThreads.cs);
  InitializeCriticalSection(&numOnce.cs);

  for (j = 0; j < NUM_ONCE; j++)
    {
      once[j] = o;

      for (i = 0; i < NUM_THREADS; i++)
        {
          /* GCC build: create was failing with EAGAIN after 790 threads */
          while (0 != pthread_create(&t[i][j], NULL, mythread, (void *)(size_t)j))
            sched_yield();
        }
    }

  for (j = 0; j < NUM_ONCE; j++)
    for (i = 0; i < NUM_THREADS; i++)
      if (pthread_join(t[i][j], NULL) != 0)
        printf("Join failed for [thread,once] = [%d,%d]\n", i, j);

  /*
   * All threads will cancel, none will return normally from
   * pthread_once and so numThreads should never be incremented. However,
   * numOnce should be incremented by every thread (NUM_THREADS*NUM_ONCE).
   */
  assert(numOnce.i == NUM_ONCE * NUM_THREADS);
  assert(numThreads.i == 0);

  DeleteCriticalSection(&numOnce.cs);
  DeleteCriticalSection(&numThreads.cs);

  return 0;
}
