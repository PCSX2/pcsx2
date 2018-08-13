/*
 * affinity1.c
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
 * Basic test of CPU_*() support routines.
 *
 */

#if ! defined(WINCE)

#include "test.h"

int
main()
{
  unsigned int cpu;
  cpu_set_t newmask;
  cpu_set_t src1mask;
  cpu_set_t src2mask;
  cpu_set_t src3mask;

  CPU_ZERO(&newmask);
  CPU_ZERO(&src1mask);
  memset(&src2mask, 0, sizeof(cpu_set_t));
  assert(memcmp(&src1mask, &src2mask, sizeof(cpu_set_t)) == 0);
  assert(CPU_EQUAL(&src1mask, &src2mask));
  assert(CPU_COUNT(&src1mask) == 0);

  CPU_ZERO(&src1mask);
  CPU_ZERO(&src2mask);
  CPU_ZERO(&src3mask);

  for (cpu = 0; cpu < sizeof(cpu_set_t)*8; cpu += 2)
    {
	  CPU_SET(cpu, &src1mask);					/* 0b01010101010101010101010101010101 */
    }
  for (cpu = 0; cpu < sizeof(cpu_set_t)*4; cpu++)
    {
	  CPU_SET(cpu, &src2mask);					/* 0b00000000000000001111111111111111 */
    }
  for (cpu = sizeof(cpu_set_t)*4; cpu < sizeof(cpu_set_t)*8; cpu += 2)
  {
	  CPU_SET(cpu, &src2mask);					/* 0b01010101010101011111111111111111 */
  }
  for (cpu = 0; cpu < sizeof(cpu_set_t)*8; cpu += 2)
    {
	  CPU_SET(cpu, &src3mask);					/* 0b01010101010101010101010101010101 */
    }

  assert(CPU_COUNT(&src1mask) == (sizeof(cpu_set_t)*4));
  assert(CPU_COUNT(&src2mask) == ((sizeof(cpu_set_t)*4 + (sizeof(cpu_set_t)*2))));
  assert(CPU_COUNT(&src3mask) == (sizeof(cpu_set_t)*4));
  CPU_SET(0, &newmask);
  CPU_SET(1, &newmask);
  CPU_SET(3, &newmask);
  assert(CPU_ISSET(1, &newmask));
  CPU_CLR(1, &newmask);
  assert(!CPU_ISSET(1, &newmask));
  CPU_OR(&newmask, &src1mask, &src2mask);
  assert(CPU_EQUAL(&newmask, &src2mask));
  CPU_AND(&newmask, &src1mask, &src2mask);
  assert(CPU_EQUAL(&newmask, &src1mask));
  CPU_XOR(&newmask, &src1mask, &src3mask);
  memset(&src2mask, 0, sizeof(cpu_set_t));
  assert(memcmp(&newmask, &src2mask, sizeof(cpu_set_t)) == 0);

  /*
   * Need to confirm the bitwise logical right-shift in CpuCount().
   * i.e. zeros inserted into MSB on shift because cpu_set_t is
   * unsigned.
   */
  CPU_ZERO(&src1mask);
  for (cpu = 1; cpu < sizeof(cpu_set_t)*8; cpu += 2)
    {
	  CPU_SET(cpu, &src1mask);					/* 0b10101010101010101010101010101010 */
    }
  assert(CPU_ISSET(sizeof(cpu_set_t)*8-1, &src1mask));
  assert(CPU_COUNT(&src1mask) == (sizeof(cpu_set_t)*4));

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
