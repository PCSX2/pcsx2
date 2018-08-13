/*
 * File: semaphore3.c
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
 * Test Synopsis: Verify sem_getvalue returns the correct number of waiters.
 * -
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
 * - Process returns zero exit status.
 *
 * Fail Criteria:
 * - Process returns non-zero exit status.
 */

#include "test.h"

#define MAX_COUNT 100

sem_t s;

void *
thr (void * arg)
{
  assert(sem_wait(&s) == 0);
  return NULL;
}

int
main()
{
  int value = 0;
  int i;
  pthread_t t[MAX_COUNT+1];

  assert(sem_init(&s, PTHREAD_PROCESS_PRIVATE, 0) == 0);
  assert(sem_getvalue(&s, &value) == 0);
  //printf("Value = %d\n", value);	fflush(stdout);
  assert(value == 0);

  for (i = 1; i <= MAX_COUNT; i++)
    {
      assert(pthread_create(&t[i], NULL, thr, NULL) == 0);
      do
        {
          sched_yield();
          assert(sem_getvalue(&s, &value) == 0);
        }
      while (-value != i);
      //printf("1:Value = %d\n", value); fflush(stdout);
      assert(-value == i);
    }

  for (i = MAX_COUNT - 1; i >= 0; i--)
    {
      assert(sem_post(&s) == 0);
      assert(sem_getvalue(&s, &value) == 0);
      //printf("2:Value = %d\n", value);	fflush(stdout);
      assert(-value == i);
    }

  for (i = MAX_COUNT; i > 0; i--)
    {
      pthread_join(t[i], NULL);
    }
  return 0;
}
