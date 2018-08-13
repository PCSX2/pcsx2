/*
 * tsd3.c
 *
 * Test Thread Specific Data (TSD) key creation and destruction.
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
 *
 * --------------------------------------------------------------------------
 *
 * Description:
 * -
 *
 * Test Method (validation or falsification):
 * - validation
 *
 * Requirements Tested:
 * - keys are created for each existing thread including the main thread
 * - keys are created for newly created threads
 * - keys are thread specific
 * - key is deleted before threads exit
 * - key destructor function is not called
 *
 * Features Tested:
 * -
 *
 * Cases Tested:
 * -
 *
 * Environment:
 * -
 *
 * Input:
 * - none
 *
 * Output:
 * - text to stdout
 *
 * Assumptions:
 * - already validated:     pthread_create()
 *                          pthread_once()
 * - main thread also has a POSIX thread identity
 *
 * Pass Criteria:
 *
 * Fail Criteria:
 */

#include <sched.h>
#include "test.h"

enum {
  NUM_THREADS = 100
};

static pthread_key_t key = NULL;
static int accesscount[NUM_THREADS];
static int thread_set[NUM_THREADS];
static int thread_destroyed[NUM_THREADS];
static pthread_barrier_t startBarrier;
static pthread_barrier_t progressSyncBarrier;

static void
destroy_key(void * arg)
{
  /*
   * The destructor function should not be called if the key
   * is deleted before the thread exits.
   */
  fprintf(stderr, "The key destructor was called but should not have been.\n");
  exit (1);
}

static void
setkey(void * arg)
{
  int * j = (int *) arg;

  thread_set[j - accesscount] = 1;

  assert(*j == 0);

  assert(pthread_getspecific(key) == NULL);

  assert(pthread_setspecific(key, arg) == 0);
  assert(pthread_setspecific(key, arg) == 0);
  assert(pthread_setspecific(key, arg) == 0);

  assert(pthread_getspecific(key) == arg);

  (*j)++;

  assert(*j == 1);
}

static void *
mythread(void * arg)
{
  (void) pthread_barrier_wait(&startBarrier);

  setkey(arg);
  (void) pthread_barrier_wait(&progressSyncBarrier);
  (void) pthread_barrier_wait(&progressSyncBarrier);

  return 0;
}

int
main()
{
  int i;
  int fail = 0;
  pthread_t thread[NUM_THREADS];

  assert(pthread_barrier_init(&startBarrier, NULL, NUM_THREADS/2) == 0);
  assert(pthread_barrier_init(&progressSyncBarrier, NULL, NUM_THREADS) == 0);

  for (i = 1; i < NUM_THREADS/2; i++)
    {
      accesscount[i] = thread_set[i] = thread_destroyed[i] = 0;
      assert(pthread_create(&thread[i], NULL, mythread, (void *)&accesscount[i]) == 0);
    }

  /*
   * Here we test that existing threads will get a key created
   * for them.
   */
  assert(pthread_key_create(&key, destroy_key) == 0);

  (void) pthread_barrier_wait(&startBarrier);

  /*
   * Test main thread key.
   */
  accesscount[0] = 0;
  setkey((void *) &accesscount[0]);

  /*
   * Here we test that new threads will get a key created
   * for them.
   */
  for (i = NUM_THREADS/2; i < NUM_THREADS; i++)
    {
      accesscount[i] = thread_set[i] = thread_destroyed[i] = 0;
      assert(pthread_create(&thread[i], NULL, mythread, (void *)&accesscount[i]) == 0);
    }

  (void) pthread_barrier_wait(&progressSyncBarrier);
  /*
   * Deleting the key should not call the key destructor.
   */
  assert(pthread_key_delete(key) == 0);
  (void) pthread_barrier_wait(&progressSyncBarrier);

  /*
   * Wait for all threads to complete.
   */
  for (i = 1; i < NUM_THREADS; i++)
    {
	assert(pthread_join(thread[i], NULL) == 0);
    }

  assert(pthread_barrier_destroy(&startBarrier) == 0);
  assert(pthread_barrier_destroy(&progressSyncBarrier) == 0);

  for (i = 1; i < NUM_THREADS; i++)
    {
      /*
       * The counter is incremented once when the key is set to
       * a value.
       */
      if (accesscount[i] != 1)
        {
          fail++;
          fprintf(stderr, "Thread %d key, set = %d, destroyed = %d\n",
              i, thread_set[i], thread_destroyed[i]);
        }
    }

  fflush(stderr);

  return (fail);
}
