/*
 * Test for pthread_timedjoin_np() timing out.
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
 * Depends on API functions: pthread_create().
 */

#include "test.h"

void *
func(void * arg)
{
        Sleep(1200);
        return arg;
}

int
main(int argc, char * argv[])
{
        pthread_t id;
        struct timespec abstime, reltime = { 1, 0 };
        void* result = (void*)-1;

        assert(pthread_create(&id, NULL, func, (void *)(size_t)999) == 0);

        /*
         * Let thread start before we attempt to join it.
         */
        Sleep(100);

        (void) pthread_win32_getabstime_np(&abstime, &reltime);

        /* Test for pthread_timedjoin_np timeout */
        assert(pthread_timedjoin_np(id, &result, &abstime) == ETIMEDOUT);
        assert((int)(size_t)result == -1);

        /* Test for pthread_tryjoin_np behaviour before thread has exited */
        assert(pthread_tryjoin_np(id, &result) == EBUSY);
        assert((int)(size_t)result == -1);

        Sleep(500);

        /* Test for pthread_tryjoin_np behaviour after thread has exited */
        assert(pthread_tryjoin_np(id, &result) == 0);
        assert((int)(size_t)result == 999);

        /* Success. */
        return 0;
}
