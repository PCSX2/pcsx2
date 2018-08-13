/* 
 * rwlock3_t.c
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
 * Declare a static rwlock object, timed-wrlock it, trywrlock it, 
 * and then unlock it again.
 *
 * Depends on API functions: 
 *	pthread_rwlock_timedwrlock()
 *	pthread_rwlock_trywrlock()
 *	pthread_rwlock_unlock()
 */

#include "test.h"
#include <sys/timeb.h>

pthread_rwlock_t rwlock1 = PTHREAD_RWLOCK_INITIALIZER;

static int washere = 0;

void * func(void * arg)
{
  assert(pthread_rwlock_trywrlock(&rwlock1) == EBUSY);

  washere = 1;

  return 0; 
}
 
int
main()
{
  pthread_t t;
  struct timespec abstime, reltime = { 1, 0 };

  (void) pthread_win32_getabstime_np(&abstime, &reltime);

  assert(pthread_rwlock_timedwrlock(&rwlock1, &abstime) == 0);

  assert(pthread_create(&t, NULL, func, NULL) == 0);

  Sleep(2000);

  assert(pthread_rwlock_unlock(&rwlock1) == 0);

  assert(washere == 1);

  return 0;
}
