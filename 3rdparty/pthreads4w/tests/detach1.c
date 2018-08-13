/*
 * Test for pthread_detach().
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
 * Depends on API functions: pthread_create(), pthread_detach(), pthread_exit().
 */

#include "test.h"


enum {
  NUMTHREADS = 100
};

void *
func(void * arg)
{
    int i = (int)(size_t)arg;

    Sleep(i * 10);

    pthread_exit(arg);

    /* Never reached. */
    exit(1);
}

int
main(int argc, char * argv[])
{
	pthread_t id[NUMTHREADS];
	int i;

	/* Create a few threads and then exit. */
	for (i = 0; i < NUMTHREADS; i++)
	  {
	    assert(pthread_create(&id[i], NULL, func, (void *)(size_t)i) == 0);
	  }

	/* Some threads will finish before they are detached, some after. */
	Sleep(NUMTHREADS/2 * 10 + 50);

	for (i = 0; i < NUMTHREADS; i++)
	  {
	    assert(pthread_detach(id[i]) == 0);
	  }

	Sleep(NUMTHREADS * 10 + 100);

	/*
	 * Check that all threads are now invalid.
	 * This relies on unique thread IDs - e.g. works with
	 * Pthreads4w or Solaris, but may not work for Linux, BSD etc.
	 */
	for (i = 0; i < NUMTHREADS; i++)
	  {
	    assert(pthread_kill(id[i], 0) == ESRCH);
	  }

	/* Success. */
	return 0;
}
