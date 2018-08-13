/*
 * File: semaphore1.c
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
 * Test Synopsis: Verify trywait() returns -1 and sets EAGAIN.
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

void *
thr(void * arg)
{
  sem_t s;
  int result;

  assert(sem_init(&s, PTHREAD_PROCESS_PRIVATE, 0) == 0);

  assert((result = sem_trywait(&s)) == -1);

  if ( result == -1 )
  {
	  int err =
#if defined(PTW32_USES_SEPARATE_CRT)
	  GetLastError();
#else
      errno;
#endif
    if (err != EAGAIN)
    {
      printf("thread: sem_trywait 1: expecting error %s: got %s\n",
	     error_string[EAGAIN], error_string[err]); fflush(stdout);
    }
    assert(err == EAGAIN);
  }
  else
  {
    printf("thread: ok 1\n");
  }

  assert((result = sem_post(&s)) == 0);

  assert((result = sem_trywait(&s)) == 0);

  assert(sem_post(&s) == 0);

  return NULL;
}


int
main()
{
  pthread_t t;
  sem_t s;
  void* result1 = (void*)-1;
  int result2;

  assert(pthread_create(&t, NULL, thr, NULL) == 0);
  assert(pthread_join(t, &result1) == 0);
  assert((int)(size_t)result1 == 0);

  assert(sem_init(&s, PTHREAD_PROCESS_PRIVATE, 0) == 0);

  assert((result2 = sem_trywait(&s)) == -1);

  if (result2 == -1)
  {
    int err =
#if defined(PTW32_USES_SEPARATE_CRT)
      GetLastError();
#else
      errno;
#endif
    if (err != EAGAIN)
    {
      printf("main: sem_trywait 1: expecting error %s: got %s\n",
	     error_string[EAGAIN], error_string[err]); fflush(stdout);
    }
    assert(err == EAGAIN);
  }
  else
  {
    printf("main: ok 1\n");
  }

  assert((result2 = sem_post(&s)) == 0);

  assert((result2 = sem_trywait(&s)) == 0);

  assert(sem_post(&s) == 0);

  return 0;
}

