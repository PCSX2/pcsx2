/*
  timefn.h - portable time measurement functions
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

#ifndef TIMEFN
#define TIMEFN

#if defined(__cplusplus)
extern "C" {
#endif

/*-****************************************
 *  Types
 ******************************************/

typedef unsigned long long Duration_ns;

/* TIME_t contains a nanosecond time counter.
 * The absolute value is not meaningful.
 * It's only valid to compute Duration_ns between 2 measurements. */
typedef struct {
    Duration_ns t;
} TIME_t;
#define TIME_INITIALIZER { 0 }

/*-****************************************
 *  Time functions
 ******************************************/

/* @return a TIME_t value to be compared to another one in order to compute a duration.
 * The absolute value returned is meaningless */
TIME_t TIME_getTime(void);

/* Timer resolution can be low on some platforms.
 * To improve accuracy, it's recommended to wait for a new tick
 * before starting benchmark measurements */
void TIME_waitForNextTick(void);

/* tells if TIME_getTime() returns correct time measurements
 * in scenarios involving multi-threaded workload.
 * note : this is not the case if only C90 clock_t measurements are available */
int TIME_support_MT_measurements(void);

Duration_ns TIME_span_ns(TIME_t clockStart, TIME_t clockEnd);
Duration_ns TIME_clockSpan_ns(TIME_t clockStart);

#if defined(__cplusplus)
}
#endif

#endif /* TIMEFN */
