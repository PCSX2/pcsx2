/*
 * ptw32_threadDestroy.c
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


void
ptw32_threadDestroy (pthread_t thread)
{
  ptw32_thread_t * tp = (ptw32_thread_t *) thread.p;
  ptw32_thread_t threadCopy;

  if (tp != NULL)
    {
      /*
       * Copy thread state so that the thread can be atomically NULLed.
       */
      memcpy (&threadCopy, tp, sizeof (threadCopy));

      /*
       * Thread ID structs are never freed. They're NULLed and reused.
       * This also sets the thread to PThreadStateInitial (invalid).
       */
      ptw32_threadReusePush (thread);

      /* Now work on the copy. */
      if (threadCopy.cancelEvent != NULL)
	{
	  CloseHandle (threadCopy.cancelEvent);
	}

#if ! defined(__MINGW32__) || defined (__MSVCRT__) || defined (__DMC__)
      /*
       * See documentation for endthread vs endthreadex.
       */
      if (threadCopy.threadH != 0)
	{
	  CloseHandle (threadCopy.threadH);
	}
#endif

    }
}				/* ptw32_threadDestroy */

