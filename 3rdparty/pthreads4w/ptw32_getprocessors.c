/*
 * ptw32_getprocessors.c
 *
 * Description:
 * This translation unit implements routines which are private to
 * the implementation and may be used throughout it.
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
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "pthread.h"
#include "implement.h"


/*
 * ptw32_getprocessors()
 *
 * Get the number of CPUs available to the process.
 *
 * If the available number of CPUs is 1 then pthread_spin_lock()
 * will block rather than spin if the lock is already owned.
 *
 * pthread_spin_init() calls this routine when initialising
 * a spinlock. If the number of available processors changes
 * (after a call to SetProcessAffinityMask()) then only
 * newly initialised spinlocks will notice.
 */
int
ptw32_getprocessors (int *count)
{
  DWORD_PTR vProcessCPUs;
  DWORD_PTR vSystemCPUs;
  int result = 0;

#if defined(NEED_PROCESS_AFFINITY_MASK)

  *count = 1;

#else

  if (GetProcessAffinityMask (GetCurrentProcess (),
			      &vProcessCPUs, &vSystemCPUs))
    {
      DWORD_PTR bit;
      int CPUs = 0;

      for (bit = 1; bit != 0; bit <<= 1)
	{
	  if (vProcessCPUs & bit)
	    {
	      CPUs++;
	    }
	}
      *count = CPUs;
    }
  else
    {
      result = EAGAIN;
    }

#endif

  return (result);
}
