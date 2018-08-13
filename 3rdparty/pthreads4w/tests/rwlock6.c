/*
 * rwlock6.c
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
 * Check writer and reader locking
 *
 * Depends on API functions:
 *      pthread_rwlock_rdlock()
 *      pthread_rwlock_wrlock()
 *      pthread_rwlock_unlock()
 */

#include "test.h"

static pthread_rwlock_t rwlock1 = PTHREAD_RWLOCK_INITIALIZER;

static int bankAccount = 0;

void * wrfunc(void * arg)
{
  int ba;

  assert(pthread_rwlock_wrlock(&rwlock1) == 0);
  Sleep(200);
  bankAccount += 10;
  ba = bankAccount;
  assert(pthread_rwlock_unlock(&rwlock1) == 0);

  return ((void *)(size_t)ba);
}

void * rdfunc(void * arg)
{
  int ba;

  assert(pthread_rwlock_rdlock(&rwlock1) == 0);
  ba = bankAccount;
  assert(pthread_rwlock_unlock(&rwlock1) == 0);

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

  bankAccount = 0;

  assert(pthread_create(&wrt1, NULL, wrfunc, NULL) == 0);
  Sleep(50);
  assert(pthread_create(&rdt, NULL, rdfunc, NULL) == 0);
  Sleep(50);
  assert(pthread_create(&wrt2, NULL, wrfunc, NULL) == 0);

  assert(pthread_join(wrt1, &wr1Result) == 0);
  assert(pthread_join(rdt, &rdResult) == 0);
  assert(pthread_join(wrt2, &wr2Result) == 0);

  assert((int)(size_t)wr1Result == 10);
  assert((int)(size_t)rdResult == 10);
  assert((int)(size_t)wr2Result == 20);

  return 0;
}
