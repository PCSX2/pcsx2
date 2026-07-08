/*
    util.h - utility functions
    Copyright (c) Yann Collet. All rights reserved.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#if defined (__cplusplus)
extern "C" {
#endif


/*-****************************************
*  Dependencies
******************************************/
#include "util.h"   /* note : ensure that platform.h is included first ! */

/*-****************************************
*  count the number of cores
******************************************/

#if defined(_WIN32)

#include <windows.h>

int UTIL_countCores(void)
{
    static int numCores = 0;
    if (numCores != 0) return numCores;

    {   SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);
        numCores = sysinfo.dwNumberOfProcessors;
    }

    if (numCores == 0) {
        /* Unexpected result, fall back on 1 */
        return numCores = 1;
    }

    return numCores;
}

#elif defined(__APPLE__)

#include <sys/sysctl.h>

/* Use apple-provided syscall
 * see: man 3 sysctl */
int UTIL_countCores(void)
{
    static S32 numCores = 0; /* apple specifies int32_t */
    if (numCores != 0) return (int)numCores;

    {   size_t size = sizeof(S32);
        int const ret = sysctlbyname("hw.logicalcpu", &numCores, &size, NULL, 0);
        if (ret != 0) {
            /* error: fall back on 1 */
            numCores = 1;
        }
    }
    return (int)numCores;
}

#elif defined(__linux__)

int UTIL_countCores(void)
{
    static int numCores = 0;

    if (numCores != 0) return numCores;

    numCores = (int)sysconf(_SC_NPROCESSORS_ONLN);
    if (numCores == -1) {
        /* value not queryable, fall back on 1 */
        return numCores = 1;
    }

    return numCores;
}

#elif defined(__FreeBSD__)

#include <stdio.h>  /* perror */
#include <errno.h>
#include <sys/param.h>
#include <sys/sysctl.h>

/* Use physical core sysctl when available
 * see: man 4 smp, man 3 sysctl */
int UTIL_countCores(void)
{
    static int numCores = 0; /* freebsd sysctl is native int sized */
    if (numCores != 0) return numCores;

#if __FreeBSD_version >= 1300008
    {   size_t size = sizeof(numCores);
        int ret = sysctlbyname("kern.smp.cores", &numCores, &size, NULL, 0);
        if (ret == 0) {
            int perCore = 1;
            ret = sysctlbyname("kern.smp.threads_per_core", &perCore, &size, NULL, 0);
            /* default to physical cores if logical cannot be read */
            if (ret != 0) /* error */
                return numCores;
            numCores *= perCore;
            return numCores;
        }
        if (errno != ENOENT) {
            perror("lz4: can't get number of cpus");
            exit(1);
        }
        /* sysctl not present, fall through to older sysconf method */
    }
#endif

    numCores = (int)sysconf(_SC_NPROCESSORS_ONLN);
    if (numCores == -1) {
        /* value not queryable, fall back on 1 */
        numCores = 1;
    }
    return numCores;
}

#elif defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__) || defined(__CYGWIN__)

/* Use POSIX sysconf
 * see: man 3 sysconf */
int UTIL_countCores(void)
{
    static int numCores = 0;
    if (numCores != 0) return numCores;

    numCores = (int)sysconf(_SC_NPROCESSORS_ONLN);
    if (numCores == -1) {
        /* value not queryable, fall back on 1 */
        numCores = 1;
    }
    return numCores;
}

#else

int UTIL_countCores(void)
{
    /* no clue */
    return 1;
}

#endif

#if defined (__cplusplus)
}
#endif
