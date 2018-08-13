/*
 * barrier6.c
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
 * Destroy the barrier after initial count threads are released then let
 * additional threads attempt to wait on it.
 *
 */

#include "test.h"

enum {
  NUMTHREADS = 31
};
 
pthread_barrier_t barrier = NULL;
pthread_mutex_t mx = PTHREAD_MUTEX_INITIALIZER;
static int serialThreadCount = 0;
static int otherThreadCount = 0;

void *
func(void * arg)
{
  int result = pthread_barrier_wait(&barrier);

  assert(pthread_mutex_lock(&mx) == 0);

  if (result == PTHREAD_BARRIER_SERIAL_THREAD)
    {
      serialThreadCount++;
    }
  else if (0 == result)
    {
      otherThreadCount++;
    }
  assert(pthread_mutex_unlock(&mx) == 0);

  return NULL;
}

int
main()
{
  int i, j, k;
  pthread_t t[NUMTHREADS + 1];

  for (j = 1; j <= NUMTHREADS; j++)
    {
      int howHigh = j/2 + 1;

      printf("Barrier height = %d, Total threads %d\n", howHigh, j);

      serialThreadCount = 0;
      otherThreadCount = 0;

      assert(pthread_barrier_init(&barrier, NULL, howHigh) == 0);

      for (i = 1; i <= j; i++)
        {
          assert(pthread_create(&t[i], NULL, func, NULL) == 0);

          if (i == howHigh)
            {
              for (k = 1; k <= howHigh; k++)
                {
                  assert(pthread_join(t[k], NULL) == 0);
                }
              assert(pthread_barrier_destroy(&barrier) == 0);
            }
        }

      for (i = howHigh+1; i <= j; i++)
        {
          assert(pthread_join(t[i], NULL) == 0);
        }

      assert(serialThreadCount == 1);
      assert(otherThreadCount == (howHigh - 1));

      assert(pthread_barrier_destroy(&barrier) == EINVAL);
    }

  assert(pthread_mutex_destroy(&mx) == 0);

  return 0;
}
