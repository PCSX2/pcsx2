/*
 * name_np2.c
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
 * Description:
 * Create a thread and give it a name.
 *
 * The MSVC version should display the thread name in the MSVS debugger.
 * Confirmed for MSVS10 Express:
 *
 *      VCExpress name_np1.exe /debugexe
 *
 * did indeed display the thread name in the trace output.
 *
 * Depends on API functions:
 *      pthread_create
 *      pthread_join
 *      pthread_self
 *      pthread_attr_init
 *      pthread_getname_np
 *      pthread_attr_setname_np
 *      pthread_barrier_init
 *      pthread_barrier_wait
 */

#include "test.h"

static int washere = 0;
static pthread_attr_t attr;
static pthread_barrier_t sync;
#if defined(PTW32_COMPATIBILITY_BSD)
static int seqno = 0;
#endif

void * func(void * arg)
{
  char buf[32];
  pthread_t self = pthread_self();

  washere = 1;
  pthread_barrier_wait(&sync);
  assert(pthread_getname_np(self, buf, 32) == 0);
  printf("Thread name: %s\n", buf);
  pthread_barrier_wait(&sync);

  return 0;
}

int
main()
{
  pthread_t t;

  assert(pthread_attr_init(&attr) == 0);
#if defined(PTW32_COMPATIBILITY_BSD)
  seqno++;
  assert(pthread_attr_setname_np(&attr, "MyThread%d", (void *)&seqno) == 0);
#elif defined(PTW32_COMPATIBILITY_TRU64)
  assert(pthread_attr_setname_np(&attr, "MyThread1", NULL) == 0);
#else
  assert(pthread_attr_setname_np(&attr, "MyThread1") == 0);
#endif

  assert(pthread_barrier_init(&sync, NULL, 2) == 0);

  assert(pthread_create(&t, &attr, func, NULL) == 0);
  pthread_barrier_wait(&sync);
  pthread_barrier_wait(&sync);

  assert(pthread_join(t, NULL) == 0);

  assert(pthread_barrier_destroy(&sync) == 0);
  assert(pthread_attr_destroy(&attr) == 0);

  assert(washere == 1);

  return 0;
}
