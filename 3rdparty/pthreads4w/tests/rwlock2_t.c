/* 
 * rwlock2_t.c
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
 * Declare a static rwlock object, timed-lock it, 
 * and then unlock it again.
 *
 * Depends on API functions:
 *	pthread_rwlock_timedrdlock()
 *	pthread_rwlock_unlock()
 */

#include "test.h"
#include <sys/timeb.h>

pthread_rwlock_t rwlock = PTHREAD_RWLOCK_INITIALIZER;

int
main()
{
  struct timespec abstime, reltime = { 1, 0 };

  (void) pthread_win32_getabstime_np(&abstime, &reltime);

  assert(rwlock == PTHREAD_RWLOCK_INITIALIZER);

  assert(pthread_rwlock_timedrdlock(&rwlock, &abstime) == 0);

  assert(rwlock != PTHREAD_RWLOCK_INITIALIZER);

  assert(rwlock != NULL);

  assert(pthread_rwlock_unlock(&rwlock) == 0);

  assert(pthread_rwlock_destroy(&rwlock) == 0);

  assert(rwlock == NULL);

  return 0;
}
