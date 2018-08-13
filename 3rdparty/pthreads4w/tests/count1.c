/*
 * count1.c
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
 * Description:
 * Test some basic assertions about the number of threads at runtime.
 */

#include "test.h"

#define NUMTHREADS (30)

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_t threads[NUMTHREADS];
static unsigned numThreads = 0;

void *
myfunc(void *arg)
{
  pthread_mutex_lock(&lock);
  numThreads++;
  pthread_mutex_unlock(&lock);

  Sleep(1000);
  return 0;
}
int
main()
{
  int i;
  int maxThreads = sizeof(threads) / sizeof(pthread_t);

  /*
   * Spawn NUMTHREADS threads. Each thread should increment the
   * numThreads variable, sleep for one second.
   */
  for (i = 0; i < maxThreads; i++)
    {
      assert(pthread_create(&threads[i], NULL, myfunc, 0) == 0);
    }
  
  /*
   * Wait for all the threads to exit.
   */
  for (i = 0; i < maxThreads; i++)
    {
      assert(pthread_join(threads[i], NULL) == 0);
    }

  /* 
   * Check the number of threads created.
   */
  assert((int) numThreads == maxThreads);
  
  /*
   * Success.
   */
  return 0;
}
