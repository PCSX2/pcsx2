/*
 * pthread_getspecific.c
 *
 * Description:
 * POSIX thread functions which implement thread-specific data (TSD).
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


void *
pthread_getspecific (pthread_key_t key)
     /*
      * ------------------------------------------------------
      * DOCPUBLIC
      *      This function returns the current value of key in the
      *      calling thread. If no value has been set for 'key' in 
      *      the thread, NULL is returned.
      *
      * PARAMETERS
      *      key
      *              an instance of pthread_key_t
      *
      *
      * DESCRIPTION
      *      This function returns the current value of key in the
      *      calling thread. If no value has been set for 'key' in 
      *      the thread, NULL is returned.
      *
      * RESULTS
      *              key value or NULL on failure
      *
      * ------------------------------------------------------
      */
{
  void * ptr;

  if (key == NULL)
    {
      ptr = NULL;
    }
  else
    {
      int lasterror = GetLastError ();
#if defined(RETAIN_WSALASTERROR)
      int lastWSAerror = WSAGetLastError ();
#endif
      ptr = TlsGetValue (key->key);

      SetLastError (lasterror);
#if defined(RETAIN_WSALASTERROR)
      WSASetLastError (lastWSAerror);
#endif
    }

  return ptr;
}
