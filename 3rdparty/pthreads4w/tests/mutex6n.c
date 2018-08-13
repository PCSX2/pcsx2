/* 
 * mutex6n.c
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
 * Tests PTHREAD_MUTEX_NORMAL mutex type.
 * Thread locks mutex twice (recursive lock).
 * The thread should deadlock.
 *
 * Depends on API functions: 
 *      pthread_create()
 *      pthread_mutexattr_init()
 *      pthread_mutexattr_settype()
 *      pthread_mutexattr_gettype()
 *      pthread_mutex_init()
 *	pthread_mutex_lock()
 *	pthread_mutex_unlock()
 */

#include "test.h"

static int lockCount;

static pthread_mutex_t mutex;
static pthread_mutexattr_t mxAttr;

void * locker(void * arg)
{
  assert(pthread_mutex_lock(&mutex) == 0);
  lockCount++;

  /* Should wait here (deadlocked) */
  assert(pthread_mutex_lock(&mutex) == 0);

  lockCount++;
  assert(pthread_mutex_unlock(&mutex) == 0);

  return (void *) 555;
}
 
int
main()
{
  pthread_t t;
  int mxType = -1;

  assert(pthread_mutexattr_init(&mxAttr) == 0);

  BEGIN_MUTEX_STALLED_ROBUST(mxAttr)

  lockCount = 0;
  assert(pthread_mutexattr_settype(&mxAttr, PTHREAD_MUTEX_NORMAL) == 0);
  assert(pthread_mutexattr_gettype(&mxAttr, &mxType) == 0);
  assert(mxType == PTHREAD_MUTEX_NORMAL);

  assert(pthread_mutex_init(&mutex, &mxAttr) == 0);

  assert(pthread_create(&t, NULL, locker, NULL) == 0);

  while (lockCount < 1)
    {
      Sleep(1);
    }

  assert(lockCount == 1);

  assert(pthread_mutex_unlock(&mutex) == (IS_ROBUST?EPERM:0));

  while (lockCount < (IS_ROBUST?1:2))
    {
      Sleep(1);
    }

  assert(lockCount == (IS_ROBUST?1:2));

  END_MUTEX_STALLED_ROBUST(mxAttr)

  return 0;
}

