/* 
 * mutex5.c
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
 *
 * --------------------------------------------------------------------------
 *
 * Confirm the equality/inequality of the various mutex types,
 * and the default not-set value.
 */

#include "test.h"

static pthread_mutexattr_t mxAttr;

/* Prevent optimiser from removing dead or obvious asserts. */
int _optimiseFoil;
#define FOIL(x) (_optimiseFoil = x)

int
main()
{
  int mxType = -1;

  assert(FOIL(PTHREAD_MUTEX_DEFAULT) == PTHREAD_MUTEX_NORMAL);
  assert(FOIL(PTHREAD_MUTEX_DEFAULT) != PTHREAD_MUTEX_ERRORCHECK);
  assert(FOIL(PTHREAD_MUTEX_DEFAULT) != PTHREAD_MUTEX_RECURSIVE);
  assert(FOIL(PTHREAD_MUTEX_RECURSIVE) != PTHREAD_MUTEX_ERRORCHECK);

  assert(FOIL(PTHREAD_MUTEX_NORMAL) == PTHREAD_MUTEX_FAST_NP);
  assert(FOIL(PTHREAD_MUTEX_RECURSIVE) == PTHREAD_MUTEX_RECURSIVE_NP);
  assert(FOIL(PTHREAD_MUTEX_ERRORCHECK) == PTHREAD_MUTEX_ERRORCHECK_NP);

  assert(pthread_mutexattr_init(&mxAttr) == 0);
  assert(pthread_mutexattr_gettype(&mxAttr, &mxType) == 0);
  assert(mxType == PTHREAD_MUTEX_NORMAL);

  return 0;
}
