/*
 * pthread_attr_getstackaddr.c
 *
 * Description:
 * This translation unit implements operations on thread attribute objects.
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

/* ignore warning "unreferenced formal parameter" */
#if defined(_MSC_VER)
#pragma warning( disable : 4100 )
#endif

int
pthread_attr_getstackaddr (const pthread_attr_t * attr, void **stackaddr)
     /*
      * ------------------------------------------------------
      * DOCPUBLIC
      *      This function determines the address of the stack
      *      on which threads created with 'attr' will run.
      *
      * PARAMETERS
      *      attr
      *              pointer to an instance of pthread_attr_t
      *
      *      stackaddr
      *              pointer into which is returned the stack address.
      *
      *
      * DESCRIPTION
      *      This function determines the address of the stack
      *      on which threads created with 'attr' will run.
      *
      *      NOTES:
      *              1)      Function supported only if this macro is
      *                      defined:
      *
      *                              _POSIX_THREAD_ATTR_STACKADDR
      *
      *              2)      Create only one thread for each stack
      *                      address..
      *
      * RESULTS
      *              0               successfully retrieved stack address,
      *              EINVAL          'attr' is invalid
      *              ENOSYS          function not supported
      *
      * ------------------------------------------------------
      */
{
#if defined( _POSIX_THREAD_ATTR_STACKADDR ) && _POSIX_THREAD_ATTR_STACKADDR != -1

  if (ptw32_is_attr (attr) != 0)
    {
      return EINVAL;
    }

  *stackaddr = (*attr)->stackaddr;
  return 0;

#else

  return ENOSYS;

#endif /* _POSIX_THREAD_ATTR_STACKADDR */
}
