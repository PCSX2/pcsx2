/*
 * pthread_kill.c
 *
 * Description:
 * This translation unit implements the pthread_kill routine.
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
 * Not needed yet, but defining it should indicate clashes with build target
 * environment that should be fixed.
 */
#if !defined(WINCE)
#  include <signal.h>
#endif

int
pthread_kill (pthread_t thread, int sig)
     /*
      * ------------------------------------------------------
      * DOCPUBLIC
      *      This function requests that a signal be delivered to the
      *      specified thread. If sig is zero, error checking is
      *      performed but no signal is actually sent such that this
      *      function can be used to check for a valid thread ID.
      *
      * PARAMETERS
      *      thread  reference to an instances of pthread_t
      *      sig     signal. Currently only a value of 0 is supported.
      *
      *
      * DESCRIPTION
      *      This function requests that a signal be delivered to the
      *      specified thread. If sig is zero, error checking is
      *      performed but no signal is actually sent such that this
      *      function can be used to check for a valid thread ID.
      *
      * RESULTS
      *              ESRCH           the thread is not a valid thread ID,
      *              EINVAL          the value of the signal is invalid
      *                              or unsupported.
      *              0               the signal was successfully sent.
      *
      * ------------------------------------------------------
      */
{
  int result = 0;
  ptw32_thread_t * tp;
  ptw32_mcs_local_node_t node;

  ptw32_mcs_lock_acquire(&ptw32_thread_reuse_lock, &node);

  tp = (ptw32_thread_t *) thread.p;

  if (NULL == tp
      || thread.x != tp->ptHandle.x
      || NULL == tp->threadH)
    {
      result = ESRCH;
    }

  ptw32_mcs_lock_release(&node);

  if (0 == result && 0 != sig)
    {
      /*
       * Currently does not support any signals.
       */
      result = EINVAL;
    }

  return result;

}				/* pthread_kill */
