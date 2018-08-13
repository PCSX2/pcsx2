/*
 * affinity2.c
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
 * Have the process switch CPUs.
 *
 */

#if ! defined(WINCE)

#include "test.h"

int
main()
{
  unsigned int cpu;
  int result;
  cpu_set_t newmask;
  cpu_set_t mask;
  cpu_set_t switchmask;
  cpu_set_t flipmask;

  CPU_ZERO(&mask);
  CPU_ZERO(&switchmask);
  CPU_ZERO(&flipmask);

  for (cpu = 0; cpu < sizeof(cpu_set_t)*8; cpu += 2)
    {
	  CPU_SET(cpu, &switchmask);				/* 0b01010101010101010101010101010101 */
    }
  for (cpu = 0; cpu < sizeof(cpu_set_t)*8; cpu++)
    {
	  CPU_SET(cpu, &flipmask);					/* 0b11111111111111111111111111111111 */
    }

  assert(sched_getaffinity(0, sizeof(cpu_set_t), &newmask) == 0);
  assert(!CPU_EQUAL(&newmask, &mask));

  result = sched_setaffinity(0, sizeof(cpu_set_t), &newmask);
  if (result != 0)
	{
	  int err =
#if defined(PTW32_USES_SEPARATE_CRT)
	  GetLastError();
#else
      errno;
#endif

	  assert(err != ESRCH);
	  assert(err != EFAULT);
	  assert(err != EPERM);
	  assert(err != EINVAL);
	  assert(err != EAGAIN);
	  assert(err == ENOSYS);
	  assert(CPU_COUNT(&mask) == 1);
	}
  else
	{
	  if (CPU_COUNT(&mask) > 1)
		{
		  CPU_AND(&newmask, &mask, &switchmask); /* Remove every other CPU */
		  assert(sched_setaffinity(0, sizeof(cpu_set_t), &newmask) == 0);
		  assert(sched_getaffinity(0, sizeof(cpu_set_t), &mask) == 0);
		  CPU_XOR(&newmask, &mask, &flipmask);  /* Switch to all alternative CPUs */
		  assert(sched_setaffinity(0, sizeof(cpu_set_t), &newmask) == 0);
		  assert(sched_getaffinity(0, sizeof(cpu_set_t), &mask) == 0);
		  assert(!CPU_EQUAL(&newmask, &mask));
		}
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
