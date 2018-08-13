/*
 * pthread_cond_init.c
 *
 * Description:
 * This translation unit implements condition variables and their primitives.
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
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "pthread.h"
#include "implement.h"


int
pthread_cond_init (pthread_cond_t * cond, const pthread_condattr_t * attr)
     /*
      * ------------------------------------------------------
      * DOCPUBLIC
      *      This function initializes a condition variable.
      *
      * PARAMETERS
      *      cond
      *              pointer to an instance of pthread_cond_t
      *
      *      attr
      *              specifies optional creation attributes.
      *
      *
      * DESCRIPTION
      *      This function initializes a condition variable.
      *
      * RESULTS
      *              0               successfully created condition variable,
      *              EINVAL          'attr' is invalid,
      *              EAGAIN          insufficient resources (other than
      *                              memory,
      *              ENOMEM          insufficient memory,
      *              EBUSY           'cond' is already initialized,
      *
      * ------------------------------------------------------
      */
{
  int result;
  pthread_cond_t cv = NULL;

  if (cond == NULL)
    {
      return EINVAL;
    }

  if ((attr != NULL && *attr != NULL) &&
      ((*attr)->pshared == PTHREAD_PROCESS_SHARED))
    {
      /*
       * Creating condition variable that can be shared between
       * processes.
       */
      result = ENOSYS;
      goto DONE;
    }

  cv = (pthread_cond_t) calloc (1, sizeof (*cv));

  if (cv == NULL)
    {
      result = ENOMEM;
      goto DONE;
    }

  cv->nWaitersBlocked = 0;
  cv->nWaitersToUnblock = 0;
  cv->nWaitersGone = 0;

  if (sem_init (&(cv->semBlockLock), 0, 1) != 0)
    {
      result = PTW32_GET_ERRNO();
      goto FAIL0;
    }

  if (sem_init (&(cv->semBlockQueue), 0, 0) != 0)
    {
      result = PTW32_GET_ERRNO();
      goto FAIL1;
    }

  if ((result = pthread_mutex_init (&(cv->mtxUnblockLock), 0)) != 0)
    {
      goto FAIL2;
    }

  result = 0;

  goto DONE;

  /*
   * -------------
   * Failed...
   * -------------
   */
FAIL2:
  (void) sem_destroy (&(cv->semBlockQueue));

FAIL1:
  (void) sem_destroy (&(cv->semBlockLock));

FAIL0:
  (void) free (cv);
  cv = NULL;

DONE:
  if (0 == result)
    {
      ptw32_mcs_local_node_t node;

      ptw32_mcs_lock_acquire(&ptw32_cond_list_lock, &node);

      cv->next = NULL;
      cv->prev = ptw32_cond_list_tail;

      if (ptw32_cond_list_tail != NULL)
	{
	  ptw32_cond_list_tail->next = cv;
	}

      ptw32_cond_list_tail = cv;

      if (ptw32_cond_list_head == NULL)
	{
	  ptw32_cond_list_head = cv;
	}

      ptw32_mcs_lock_release(&node);
    }

  *cond = cv;

  return result;

}				/* pthread_cond_init */
