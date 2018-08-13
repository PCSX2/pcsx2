/*
 * ptw32_tkAssocDestroy.c
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
ptw32_tkAssocDestroy (ThreadKeyAssoc * assoc)
     /*
      * -------------------------------------------------------------------
      * This routine releases all resources for the given ThreadKeyAssoc
      * once it is no longer being referenced
      * ie) either the key or thread has stopped referencing it.
      *
      * Parameters:
      *              assoc
      *                      an instance of ThreadKeyAssoc.
      * Returns:
      *      N/A
      * -------------------------------------------------------------------
      */
{

  /*
   * Both key->keyLock and thread->threadLock are locked before
   * entry to this routine.
   */
  if (assoc != NULL)
    {
      ThreadKeyAssoc * prev, * next;

      /* Remove assoc from thread's keys chain */
      prev = assoc->prevKey;
      next = assoc->nextKey;
      if (prev != NULL)
	{
	  prev->nextKey = next;
	}
      if (next != NULL)
	{
	  next->prevKey = prev;
	}

      if (assoc->thread->keys == assoc)
	{
	  /* We're at the head of the thread's keys chain */
	  assoc->thread->keys = next;
	}
      if (assoc->thread->nextAssoc == assoc)
	{
	  /*
	   * Thread is exiting and we're deleting the assoc to be processed next.
	   * Hand thread the assoc after this one.
	   */
	  assoc->thread->nextAssoc = next;
	}

      /* Remove assoc from key's threads chain */
      prev = assoc->prevThread;
      next = assoc->nextThread;
      if (prev != NULL)
	{
	  prev->nextThread = next;
	}
      if (next != NULL)
	{
	  next->prevThread = prev;
	}

      if (assoc->key->threads == assoc)
	{
	  /* We're at the head of the key's threads chain */
	  assoc->key->threads = next;
	}

      free (assoc);
    }

}				/* ptw32_tkAssocDestroy */
