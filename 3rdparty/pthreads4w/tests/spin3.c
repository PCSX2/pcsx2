/* 
 * spin3.c
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
 * Thread A locks spin - thread B tries to unlock.
 * This should succeed, but it's undefined behaviour.
 *
 */

#include "test.h"

static int wasHere = 0;

static pthread_spinlock_t spin;
 
void * unlocker(void * arg)
{
  int expectedResult = (int)(size_t)arg;

  wasHere++;
  assert(pthread_spin_unlock(&spin) == expectedResult);
  wasHere++;
  return NULL;
}
 
int
main()
{
  pthread_t t;

  wasHere = 0;
  assert(pthread_spin_init(&spin, PTHREAD_PROCESS_PRIVATE) == 0);
  assert(pthread_spin_lock(&spin) == 0);
  assert(pthread_create(&t, NULL, unlocker, (void*)0) == 0);
  assert(pthread_join(t, NULL) == 0);
  /*
   * Our spinlocks don't record the owner thread so any thread can unlock the spinlock,
   * but nor is it an error for any thread to unlock a spinlock that is not locked.
   */
  assert(pthread_spin_unlock(&spin) == 0);
  assert(pthread_spin_destroy(&spin) == 0);
  assert(wasHere == 2);

  return 0;
}
