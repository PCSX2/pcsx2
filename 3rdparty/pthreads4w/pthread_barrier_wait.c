/*
 * pthread_barrier_wait.c
 *
 * Description:
 * This translation unit implements barrier primitives.
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
pthread_barrier_wait (pthread_barrier_t * barrier)
{
  int result;
  pthread_barrier_t b;

  ptw32_mcs_local_node_t node;

  if (barrier == NULL || *barrier == (pthread_barrier_t) PTW32_OBJECT_INVALID)
    {
      return EINVAL;
    }

  ptw32_mcs_lock_acquire(&(*barrier)->lock, &node);

  b = *barrier;
  if (--b->nCurrentBarrierHeight == 0)
    {
      /*
       * We are the last thread to arrive at the barrier before it releases us.
       * Move our MCS local node to the global scope barrier handle so that the
       * last thread out (not necessarily us) can release the lock.
       */
      ptw32_mcs_node_transfer(&b->proxynode, &node);

      /*
       * Any threads that have not quite entered sem_wait below when the
       * multiple_post has completed will nevertheless continue through
       * the semaphore (barrier).
       */
      result = (b->nInitialBarrierHeight > 1
                ? sem_post_multiple (&(b->semBarrierBreeched),
				     b->nInitialBarrierHeight - 1) : 0);
    }
  else
    {
      ptw32_mcs_lock_release(&node);
      /*
       * Use the non-cancelable version of sem_wait().
       *
       * It is possible that all nInitialBarrierHeight-1 threads are
       * at this point when the last thread enters the barrier, resets
       * nCurrentBarrierHeight = nInitialBarrierHeight and leaves.
       * If pthread_barrier_destroy is called at that moment then the
       * barrier will be destroyed along with the semas.
       */
      result = ptw32_semwait (&(b->semBarrierBreeched));
    }

  if ((PTW32_INTERLOCKED_LONG)PTW32_INTERLOCKED_INCREMENT_LONG((PTW32_INTERLOCKED_LONGPTR)&b->nCurrentBarrierHeight)
		  == (PTW32_INTERLOCKED_LONG)b->nInitialBarrierHeight)
    {
      /*
       * We are the last thread to cross this barrier
       */
      ptw32_mcs_lock_release(&b->proxynode);
      if (0 == result)
        {
          result = PTHREAD_BARRIER_SERIAL_THREAD;
        }
    }

  return (result);
}
