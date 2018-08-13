/*
 * pthread_once.c
 *
 * Description:
 * This translation unit implements miscellaneous thread functions.
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

int
pthread_once (pthread_once_t * once_control, void (PTW32_CDECL *init_routine) (void))
{
  if (once_control == NULL || init_routine == NULL)
    {
      return EINVAL;
    }
  
  if ((PTW32_INTERLOCKED_LONG)PTW32_FALSE ==
      (PTW32_INTERLOCKED_LONG)PTW32_INTERLOCKED_EXCHANGE_ADD_LONG((PTW32_INTERLOCKED_LONGPTR)&once_control->done,
                                                                  (PTW32_INTERLOCKED_LONG)0)) /* MBR fence */
    {
      ptw32_mcs_local_node_t node;

      ptw32_mcs_lock_acquire((ptw32_mcs_lock_t *)&once_control->lock, &node);

      if (!once_control->done)
	{

#if defined(PTW32_CONFIG_MSVC7)
#pragma inline_depth(0)
#endif

	  pthread_cleanup_push(ptw32_mcs_lock_release, &node);
	  (*init_routine)();
	  pthread_cleanup_pop(0);

#if defined(PTW32_CONFIG_MSVC7)
#pragma inline_depth()
#endif

	  once_control->done = PTW32_TRUE;
	}

      ptw32_mcs_lock_release(&node);
    }

  return 0;

}				/* pthread_once */
