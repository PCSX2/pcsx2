/*
 * -------------------------------------------------------------
 *
 * Module: sem_wait.c
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


static void PTW32_CDECL
ptw32_sem_wait_cleanup(void * sem)
{
  sem_t s = (sem_t) sem;
  ptw32_mcs_local_node_t node;

  ptw32_mcs_lock_acquire(&s->lock, &node);
  /*
   * If sema is destroyed do nothing, otherwise:-
   * If the sema is posted between us being canceled and us locking
   * the sema again above then we need to consume that post but cancel
   * anyway. If we don't get the semaphore we indicate that we're no
   * longer waiting.
   */
  if (*((sem_t *)sem) != NULL && !(WaitForSingleObject(s->sem, 0) == WAIT_OBJECT_0))
    {
      ++s->value;
#if defined(NEED_SEM)
      if (s->value > 0)
        {
          s->leftToUnblock = 0;
        }
#else
      /*
       * Don't release the W32 sema, it doesn't need adjustment
       * because it doesn't record the number of waiters.
       */
#endif /* NEED_SEM */
    }
  ptw32_mcs_lock_release(&node);
}

int
sem_wait (sem_t * sem)
/*
 * ------------------------------------------------------
 * DOCPUBLIC
 *      This function  waits on a semaphore.
 *
 * PARAMETERS
 *      sem
 *              pointer to an instance of sem_t
 *
 * DESCRIPTION
 *      This function waits on a semaphore. If the
 *      semaphore value is greater than zero, it decreases
 *      its value by one. If the semaphore value is zero, then
 *      the calling thread (or process) is blocked until it can
 *      successfully decrease the value or until interrupted by
 *      a signal.
 *
 * RESULTS
 *              0               successfully decreased semaphore,
 *              -1              failed, error in errno
 * ERRNO
 *              EINVAL          'sem' is not a valid semaphore,
 *              ENOSYS          semaphores are not supported,
 *              EINTR           the function was interrupted by a signal,
 *              EDEADLK         a deadlock condition was detected.
 *
 * ------------------------------------------------------
 */
{
  ptw32_mcs_local_node_t node;
  int v;
  int result = 0;
  sem_t s = *sem;

  pthread_testcancel();

  ptw32_mcs_lock_acquire(&s->lock, &node);
  v = --s->value;
  ptw32_mcs_lock_release(&node);

  if (v < 0)
    {
#if defined(PTW32_CONFIG_MSVC7)
#pragma inline_depth(0)
#endif
      /* Must wait */
      pthread_cleanup_push(ptw32_sem_wait_cleanup, (void *) s);
      result = pthreadCancelableWait (s->sem);
      /* Cleanup if we're canceled or on any other error */
      pthread_cleanup_pop(result);
#if defined(PTW32_CONFIG_MSVC7)
#pragma inline_depth()
#endif
    }
#if defined(NEED_SEM)

  if (!result)
    {
      ptw32_mcs_lock_acquire(&s->lock, &node);

      if (s->leftToUnblock > 0)
        {
          --s->leftToUnblock;
          SetEvent(s->sem);
        }
      ptw32_mcs_lock_release(&node);
    }

#endif /* NEED_SEM */

  if (result != 0)
    {
      PTW32_SET_ERRNO(result);
      return -1;
    }

  return 0;

}				/* sem_wait */
