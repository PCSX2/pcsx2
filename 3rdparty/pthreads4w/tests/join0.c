/*
 * Test for pthread_join().
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
 * Depends on API functions: pthread_create(), pthread_exit().
 */

#include "test.h"

void *
func(void * arg)
{
  Sleep(2000);

  pthread_exit(arg);

  /* Never reached. */
  exit(1);
}

int
main(int argc, char * argv[])
{
  pthread_t id;
  void* result = (void*)0;

  /* Create a single thread and wait for it to exit. */
  assert(pthread_create(&id, NULL, func, (void *) 123) == 0);

  assert(pthread_join(id, &result) == 0);

  assert((int)(size_t)result == 123);

  /* Success. */
  return 0;
}
