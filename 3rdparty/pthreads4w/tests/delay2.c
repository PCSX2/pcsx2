/*
 * delay1.c
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
 * Depends on API functions:
 *    pthread_delay_np
 */

#include "test.h"

pthread_mutex_t mx = PTHREAD_MUTEX_INITIALIZER;

void *
func(void * arg)
{
  struct timespec interval = {5, 500000000L};

  assert(pthread_mutex_lock(&mx) == 0);

#ifdef _MSC_VER
#pragma inline_depth(0)
#endif
  pthread_cleanup_push(pthread_mutex_unlock, &mx);
  assert(pthread_delay_np(&interval) == 0);
  pthread_cleanup_pop(1);
#ifdef _MSC_VER
#pragma inline_depth()
#endif

  return (void *)(size_t)1;
}

int
main(int argc, char * argv[])
{
  pthread_t t;
  void* result = (void*)0;

  assert(pthread_mutex_lock(&mx) == 0);

  assert(pthread_create(&t, NULL, func, NULL) == 0);
  assert(pthread_cancel(t) == 0);

  assert(pthread_mutex_unlock(&mx) == 0);

  assert(pthread_join(t, &result) == 0);
  assert(result == (void*)PTHREAD_CANCELED);

  return 0;
}

