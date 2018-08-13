/*
 * -------------------------------------------------------------
 *
 * Module: sem_trywait.c
 *
 * Purpose:
 *	Semaphores aren't actually part of the PThreads standard.
 *	They are defined by the POSIX Standard:
 *
 *		POSIX 1003.1b-1993	(POSIX.1b)
 *
 * -------------------------------------------------------------
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
#include "semaphore.h"
#include "implement.h"


int
sem_trywait (sem_t * sem)
/*
 * ------------------------------------------------------
 * DOCPUBLIC
 *      This function tries to wait on a semaphore.
 *
 * PARAMETERS
 *      sem
 *              pointer to an instance of sem_t
 *
 * DESCRIPTION
 *      This function tries to wait on a semaphore. If the
 *      semaphore value is greater than zero, it decreases
 *      its value by one. If the semaphore value is zero, then
 *      this function returns immediately with the error EAGAIN
 *
 * RESULTS
 *              0               successfully decreased semaphore,
 *              -1              failed, error in errno
 * ERRNO
 *              EAGAIN          the semaphore was already locked,
 *              EINVAL          'sem' is not a valid semaphore,
 *              ENOTSUP         sem_trywait is not supported,
 *              EINTR           the function was interrupted by a signal,
 *              EDEADLK         a deadlock condition was detected.
 *
 * ------------------------------------------------------
 */
{
  int result = 0;
  sem_t s = *sem;
  ptw32_mcs_local_node_t node;

  ptw32_mcs_lock_acquire(&s->lock, &node);

  if (s->value > 0)
    {
      s->value--;
    }
  else
    {
      result = EAGAIN;
    }

  ptw32_mcs_lock_release(&node);

  if (result != 0)
    {
      PTW32_SET_ERRNO(result);
      return -1;
    }

  return 0;

}				/* sem_trywait */
