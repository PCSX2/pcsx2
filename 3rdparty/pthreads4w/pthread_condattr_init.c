/*
 * pthread_condattr_init.c
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
pthread_condattr_init (pthread_condattr_t * attr)
     /*
      * ------------------------------------------------------
      * DOCPUBLIC
      *      Initializes a condition variable attributes object
      *      with default attributes.
      *
      * PARAMETERS
      *      attr
      *              pointer to an instance of pthread_condattr_t
      *
      *
      * DESCRIPTION
      *      Initializes a condition variable attributes object
      *      with default attributes.
      *
      *      NOTES:
      *              1)      Use to define condition variable types
      *              2)      It is up to the application to ensure
      *                      that it doesn't re-init an attribute
      *                      without destroying it first. Otherwise
      *                      a memory leak is created.
      *
      * RESULTS
      *              0               successfully initialized attr,
      *              ENOMEM          insufficient memory for attr.
      *
      * ------------------------------------------------------
      */
{
  pthread_condattr_t attr_result;
  int result = 0;

  attr_result = (pthread_condattr_t) calloc (1, sizeof (*attr_result));

  if (attr_result == NULL)
    {
      result = ENOMEM;
    }

  *attr = attr_result;

  return result;

}				/* pthread_condattr_init */
