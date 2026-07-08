/*
  timefn.c - portable time measurement functions
  Copyright (c) Yann Collet. All rights reserved.

  GPL v2 License

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

  You can contact the author at :
  - LZ4 source repository : https://github.com/lz4/lz4
  - LZ4 public forum : https://groups.google.com/forum/#!forum/lz4c
*/

/* ===  Dependencies  === */

#include "timefn.h"
#include <time.h> /* CLOCK_MONOTONIC, TIME_UTC */

/*-****************************************
 *  Time functions
 ******************************************/

#if defined(_WIN32) /* Windows */

#    include <stdio.h>   /* perror */
#    include <stdlib.h>  /* abort */
#    include <windows.h> /* LARGE_INTEGER */

TIME_t TIME_getTime(void)
{
    static LARGE_INTEGER ticksPerSecond;
    static int init = 0;
    if (!init) {
        if (!QueryPerformanceFrequency(&ticksPerSecond)) {
            perror("timefn::QueryPerformanceFrequency");
            abort();
        }
        init = 1;
    }
    {
        TIME_t r;
        LARGE_INTEGER x;
        QueryPerformanceCounter(&x);
        r.t = (Duration_ns)(x.QuadPart * 1000000000ULL / ticksPerSecond.QuadPart);
        return r;
    }
}

#elif defined(__APPLE__) && defined(__MACH__)

#    include <mach/mach_time.h> /* mach_timebase_info_data_t, mach_timebase_info, mach_absolute_time */

TIME_t TIME_getTime(void)
{
    static mach_timebase_info_data_t rate;
    static int init = 0;
    if (!init) {
        mach_timebase_info(&rate);
        init = 1;
    }
    {
        TIME_t r;
        r.t = mach_absolute_time() * (Duration_ns)rate.numer
                / (Duration_ns)rate.denom;
        return r;
    }
}


/* POSIX.1-2001 (optional) */
#elif defined(CLOCK_MONOTONIC)

#include <stdlib.h>   /* abort */
#include <stdio.h>    /* perror */

TIME_t TIME_getTime(void)
{
    /* time must be initialized, othersize it may fail msan test.
     * No good reason, likely a limitation of timespec_get() for some target */
    struct timespec time = { 0, 0 };
    if (clock_gettime(CLOCK_MONOTONIC, &time) != 0) {
        perror("timefn::clock_gettime(CLOCK_MONOTONIC)");
        abort();
    }
    {   TIME_t r;
        r.t = (Duration_ns)time.tv_sec * 1000000000ULL
                + (Duration_ns)time.tv_nsec;
        return r;
    }
}


/* C11 requires support of timespec_get().
 * However, FreeBSD 11 claims C11 compliance while lacking timespec_get().
 * Double confirm timespec_get() support by checking the definition of TIME_UTC.
 * However, some versions of Android manage to simultaneously define TIME_UTC
 * and lack timespec_get() support... */
#elif (defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L) /* C11 */) \
        && defined(TIME_UTC) && !defined(__ANDROID__)

#    include <stdio.h>  /* perror */
#    include <stdlib.h> /* abort */

TIME_t TIME_getTime(void)
{
    /* time must be initialized, othersize it may fail msan test */
    struct timespec time = { 0, 0 };
    if (timespec_get(&time, TIME_UTC) != TIME_UTC) {
        perror("timefn::timespec_get(TIME_UTC)");
        abort();
    }
    {
        TIME_t r;
        r.t = (Duration_ns)time.tv_sec * 1000000000ULL
                + (Duration_ns)time.tv_nsec;
        return r;
    }
}

#else /* relies on standard C90 (note : clock_t produces wrong measurements \
         for multi-threaded workloads) */

TIME_t TIME_getTime(void)
{
    TIME_t r;
    r.t = (Duration_ns)clock() * 1000000000ULL / CLOCKS_PER_SEC;
    return r;
}

#    define TIME_MT_MEASUREMENTS_NOT_SUPPORTED

#endif

/* ==== Common functions, valid for all time API ==== */

Duration_ns TIME_span_ns(TIME_t clockStart, TIME_t clockEnd)
{
    return clockEnd.t - clockStart.t;
}

Duration_ns TIME_clockSpan_ns(TIME_t clockStart)
{
    TIME_t const clockEnd = TIME_getTime();
    return TIME_span_ns(clockStart, clockEnd);
}

void TIME_waitForNextTick(void)
{
    TIME_t const clockStart = TIME_getTime();
    TIME_t clockEnd;
    do {
        clockEnd = TIME_getTime();
    } while (TIME_span_ns(clockStart, clockEnd) == 0);
}

int TIME_support_MT_measurements(void)
{
#if defined(TIME_MT_MEASUREMENTS_NOT_SUPPORTED)
    return 0;
#else
    return 1;
#endif
}
