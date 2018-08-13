/*
 * rwlock6_t2.c
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
 * Check writer and reader timeouts.
 *
 * Depends on API functions: 
 *      pthread_rwlock_timedrdlock()
 *      pthread_rwlock_timedwrlock()
 *      pthread_rwlock_unlock()
 */

#include "test.h"
#include <sys/timeb.h>

static pthread_rwlock_t rwlock1 = PTHREAD_RWLOCK_INITIALIZER;

static int bankAccount = 0;
struct timespec abstime, reltime = { 1, 0 };

void * wrfunc(void * arg)
{
  int result;

  result = pthread_rwlock_timedwrlock(&rwlock1, &abstime);
  if ((int) (size_t)arg == 1)
    {
      assert(result == 0);
      Sleep(2000);
      bankAccount += 10;
      assert(pthread_rwlock_unlock(&rwlock1) == 0);
      return ((void *)(size_t)bankAccount);
    }
  else if ((int) (size_t)arg == 2)
    {
      assert(result == ETIMEDOUT);
      return ((void *) 100);
    }

  return ((void *)(size_t)-1);
}

void * rdfunc(void * arg)
{
  int ba = 0;

  assert(pthread_rwlock_timedrdlock(&rwlock1, &abstime) == ETIMEDOUT);

  return ((void *)(size_t)ba);
}

int
main()
{
  pthread_t wrt1;
  pthread_t wrt2;
  pthread_t rdt;
  void* wr1Result = (void*)0;
  void* wr2Result = (void*)0;
  void* rdResult = (void*)0;

  (void) pthread_win32_getabstime_np(&abstime, &reltime);

  bankAccount = 0;

  assert(pthread_create(&wrt1, NULL, wrfunc, (void *)(size_t)1) == 0);
  Sleep(100);
  assert(pthread_create(&rdt, NULL, rdfunc, NULL) == 0);
  Sleep(100);
  assert(pthread_create(&wrt2, NULL, wrfunc, (void *)(size_t)2) == 0);

  assert(pthread_join(wrt1, &wr1Result) == 0);
  assert(pthread_join(rdt, &rdResult) == 0);
  assert(pthread_join(wrt2, &wr2Result) == 0);

  assert((int)(size_t)wr1Result == 10);
  assert((int)(size_t)rdResult == 0);
  assert((int)(size_t)wr2Result == 100);

  return 0;
}
