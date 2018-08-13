/* 
 * spin4.c
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
 * Declare a static spinlock object, lock it, spin on it, 
 * and then unlock it again.
 */

#include "test.h"
#include <sys/timeb.h>
 
pthread_spinlock_t lock = PTHREAD_SPINLOCK_INITIALIZER;
PTW32_STRUCT_TIMEB currSysTimeStart;
PTW32_STRUCT_TIMEB currSysTimeStop;

#define GetDurationMilliSecs(_TStart, _TStop) ((_TStop.time*1000+_TStop.millitm) \
					       - (_TStart.time*1000+_TStart.millitm))

static int washere = 0;

void * func(void * arg)
{
  PTW32_FTIME(&currSysTimeStart);
  washere = 1;
  assert(pthread_spin_lock(&lock) == 0);
  assert(pthread_spin_unlock(&lock) == 0);
  PTW32_FTIME(&currSysTimeStop);

  return (void *)(size_t)GetDurationMilliSecs(currSysTimeStart, currSysTimeStop);
}
 
int
main()
{
  void* result = (void*)0;
  pthread_t t;
  int CPUs;
  PTW32_STRUCT_TIMEB sysTime;

  if ((CPUs = pthread_num_processors_np()) == 1)
    {
      printf("Test not run - it requires multiple CPUs.\n");
	exit(0);
    }

  assert(pthread_spin_lock(&lock) == 0);

  assert(pthread_create(&t, NULL, func, NULL) == 0);

  while (washere == 0)
    {
      sched_yield();
    }

  do
    {
      sched_yield();
      PTW32_FTIME(&sysTime);
    }
  while (GetDurationMilliSecs(currSysTimeStart, sysTime) <= 1000);

  assert(pthread_spin_unlock(&lock) == 0);

  assert(pthread_join(t, &result) == 0);
  assert((int)(size_t)result > 1000);

  assert(pthread_spin_destroy(&lock) == 0);

  assert(washere == 1);

  return 0;
}
