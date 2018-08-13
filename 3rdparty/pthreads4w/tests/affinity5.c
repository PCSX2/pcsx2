/*
 * affinity5.c
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
 * Test thread CPU affinity inheritance.
 *
 */

#if ! defined(WINCE)

#include "test.h"

typedef union
{
	/* Violates opacity */
	cpu_set_t cpuset;
	unsigned long int bits;  /* To stop GCC complaining about %lx args to printf */
} cpuset_to_ulint;

void *
mythread(void * arg)
{
  HANDLE threadH = GetCurrentThread();
  cpu_set_t *parentCpus = (cpu_set_t*) arg;
  cpu_set_t threadCpus;
  DWORD_PTR vThreadMask;
  cpuset_to_ulint a, b;

  assert(pthread_getaffinity_np(pthread_self(), sizeof(cpu_set_t), &threadCpus) == 0);
  assert(CPU_EQUAL(parentCpus, &threadCpus));
  vThreadMask = SetThreadAffinityMask(threadH, (*(PDWORD_PTR)&threadCpus) /* Violating Opacity */);
  assert(vThreadMask != 0);
  assert(memcmp(&vThreadMask, &threadCpus, sizeof(DWORD_PTR)) == 0);
  a.cpuset = *parentCpus;
  b.cpuset = threadCpus;
  /* Violates opacity */
  printf("CPU affinity: Parent/Thread = 0x%lx/0x%lx\n", a.bits, b.bits);

  return (void*) 0;
}

int
main()
{
  unsigned int cpu;
  pthread_t tid;
  cpu_set_t threadCpus;
  DWORD_PTR vThreadMask;
  cpu_set_t keepCpus;
  pthread_t self = pthread_self();

  if (pthread_getaffinity_np(self, sizeof(cpu_set_t), &threadCpus) == ENOSYS)
    {
      printf("pthread_get/set_affinity_np API not supported for this platform: skipping test.");
      return 0;
    }

  CPU_ZERO(&keepCpus);
  for (cpu = 1; cpu < sizeof(cpu_set_t)*8; cpu += 2)
    {
	  CPU_SET(cpu, &keepCpus);					/* 0b10101010101010101010101010101010 */
    }

  assert(pthread_getaffinity_np(self, sizeof(cpu_set_t), &threadCpus) == 0);
  if (CPU_COUNT(&threadCpus) > 1)
    {
	  assert(pthread_create(&tid, NULL, mythread, (void*)&threadCpus) == 0);
	  assert(pthread_join(tid, NULL) == 0);
	  CPU_AND(&threadCpus, &threadCpus, &keepCpus);
	  assert(pthread_setaffinity_np(self, sizeof(cpu_set_t), &threadCpus) == 0);
	  vThreadMask = SetThreadAffinityMask(GetCurrentThread(), (*(PDWORD_PTR)&threadCpus) /* Violating Opacity */);
	  assert(vThreadMask != 0);
	  assert(memcmp(&vThreadMask, &threadCpus, sizeof(DWORD_PTR)) == 0);
	  assert(pthread_create(&tid, NULL, mythread, (void*)&threadCpus) == 0);
	  assert(pthread_join(tid, NULL) == 0);
    }

  return 0;
}

#else

#include <stdio.h>

int
main()
{
  fprintf(stderr, "Test N/A for this target environment.\n");
  return 0;
}

#endif
