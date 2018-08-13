/*
 * sched_getschedparam.c
 *
 * Description:
 * POSIX thread functions that deal with thread scheduling.
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
#include "sched.h"

int
pthread_getschedparam (pthread_t thread, int *policy,
		       struct sched_param *param)
{
  int result;

  /*
   * Validate the thread id. This method works for Pthreads4w because
   * pthread_kill and pthread_t are designed to accommodate it, but the
   * method is not portable.
   */
  result = pthread_kill (thread, 0);
  if (0 != result)
    {
      return result;
    }

  if (policy == NULL)
    {
      return EINVAL;
    }

  /* Fill out the policy. */
  *policy = SCHED_OTHER;

  /*
   * This function must return the priority value set by
   * the most recent pthread_setschedparam() or pthread_create()
   * for the target thread. It must not return the actual thread
   * priority as altered by any system priority adjustments etc.
   */
  param->sched_priority = ((ptw32_thread_t *)thread.p)->sched_priority;

  return 0;
}
