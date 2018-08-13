/*
 * ptw32_spinlock_check_need_init.c
 *
 * Description:
 * This translation unit implements spin lock primitives.
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


INLINE int
ptw32_spinlock_check_need_init (pthread_spinlock_t * lock)
{
  int result = 0;
  ptw32_mcs_local_node_t node;

  /*
   * The following guarded test is specifically for statically
   * initialised spinlocks (via PTHREAD_SPINLOCK_INITIALIZER).
   */
  ptw32_mcs_lock_acquire(&ptw32_spinlock_test_init_lock, &node);

  /*
   * We got here possibly under race
   * conditions. Check again inside the critical section
   * and only initialise if the spinlock is valid (not been destroyed).
   * If a static spinlock has been destroyed, the application can
   * re-initialise it only by calling pthread_spin_init()
   * explicitly.
   */
  if (*lock == PTHREAD_SPINLOCK_INITIALIZER)
    {
      result = pthread_spin_init (lock, PTHREAD_PROCESS_PRIVATE);
    }
  else if (*lock == NULL)
    {
      /*
       * The spinlock has been destroyed while we were waiting to
       * initialise it, so the operation that caused the
       * auto-initialisation should fail.
       */
      result = EINVAL;
    }

  ptw32_mcs_lock_release(&node);

  return (result);
}
