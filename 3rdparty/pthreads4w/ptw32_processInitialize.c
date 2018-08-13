/*
 * ptw32_processInitialize.c
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


int
ptw32_processInitialize (void)
     /*
      * ------------------------------------------------------
      * DOCPRIVATE
      *      This function performs process wide initialization for
      *      the pthread library.
      *
      * PARAMETERS
      *      N/A
      *
      * DESCRIPTION
      *      This function performs process wide initialization for
      *      the pthread library.
      *      If successful, this routine sets the global variable
      *      ptw32_processInitialized to TRUE.
      *
      * RESULTS
      *              TRUE    if successful,
      *              FALSE   otherwise
      *
      * ------------------------------------------------------
      */
{
  if (ptw32_processInitialized)
    {
      return PTW32_TRUE;
    }

  /*
   * Explicitly initialise all variables from global.c
   */
  ptw32_threadReuseTop = PTW32_THREAD_REUSE_EMPTY;
  ptw32_threadReuseBottom = PTW32_THREAD_REUSE_EMPTY;
  ptw32_selfThreadKey = NULL;
  ptw32_cleanupKey = NULL;
  ptw32_cond_list_head = NULL;
  ptw32_cond_list_tail = NULL;

  ptw32_concurrency = 0;

  /* What features have been auto-detected */
  ptw32_features = 0;

  /*
   * Global [process wide] thread sequence Number
   */
  ptw32_threadSeqNumber = 0;

  /*
   * Function pointer to QueueUserAPCEx if it exists, otherwise
   * it will be set at runtime to a substitute routine which cannot unblock
   * blocked threads.
   */
  ptw32_register_cancellation = NULL;

  /*
   * Global lock for managing pthread_t struct reuse.
   */
  ptw32_thread_reuse_lock = 0;

  /*
   * Global lock for testing internal state of statically declared mutexes.
   */
  ptw32_mutex_test_init_lock = 0;

  /*
   * Global lock for testing internal state of PTHREAD_COND_INITIALIZER
   * created condition variables.
   */
  ptw32_cond_test_init_lock = 0;

  /*
   * Global lock for testing internal state of PTHREAD_RWLOCK_INITIALIZER
   * created read/write locks.
   */
  ptw32_rwlock_test_init_lock = 0;

  /*
   * Global lock for testing internal state of PTHREAD_SPINLOCK_INITIALIZER
   * created spin locks.
   */
  ptw32_spinlock_test_init_lock = 0;

  /*
   * Global lock for condition variable linked list. The list exists
   * to wake up CVs when a WM_TIMECHANGE message arrives. See
   * w32_TimeChangeHandler.c.
   */
  ptw32_cond_list_lock = 0;

  #if defined(_UWIN)
  /*
   * Keep a count of the number of threads.
   */
  pthread_count = 0;
  #endif

  ptw32_processInitialized = PTW32_TRUE;

  /*
   * Initialize Keys
   */
  if ((pthread_key_create (&ptw32_selfThreadKey, NULL) != 0) ||
      (pthread_key_create (&ptw32_cleanupKey, NULL) != 0))
    {

      ptw32_processTerminate ();
    }

  return (ptw32_processInitialized);

}				/* processInitialize */
