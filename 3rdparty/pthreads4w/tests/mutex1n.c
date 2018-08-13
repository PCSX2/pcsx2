/* 
 * mutex1n.c
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
 * As for mutex1.c but with type set to PTHREAD_MUTEX_NORMAL.
 *
 * Create a simple mutex object, lock it, unlock it, then destroy it.
 * This is the simplest test of the pthread mutex family that we can do.
 *
 * Depends on API functions:
 *	pthread_mutexattr_settype()
 * 	pthread_mutex_init()
 *	pthread_mutex_destroy()
 */

#include "test.h"

pthread_mutex_t mutex = NULL;
pthread_mutexattr_t mxAttr;

int
main()
{
  assert(pthread_mutexattr_init(&mxAttr) == 0);

  BEGIN_MUTEX_STALLED_ROBUST(mxAttr)

  assert(pthread_mutexattr_settype(&mxAttr, PTHREAD_MUTEX_NORMAL) == 0);

  assert(mutex == NULL);

  assert(pthread_mutex_init(&mutex, &mxAttr) == 0);

  assert(mutex != NULL);

  assert(pthread_mutex_lock(&mutex) == 0);

  assert(pthread_mutex_unlock(&mutex) == 0);

  assert(pthread_mutex_destroy(&mutex) == 0);

  assert(mutex == NULL);

  END_MUTEX_STALLED_ROBUST(mxAttr)

  return 0;
}
