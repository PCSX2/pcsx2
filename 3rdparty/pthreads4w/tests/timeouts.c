/*
 * File: timeouts.c
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
 * Test Synopsis:
 * - confirm accuracy of abstime calculations and timeouts
 *
 * Test Method (Validation or Falsification):
 * - time actual CV wait timeout using a sequence of increasing sub 1 second timeouts.
 *
 * Requirements Tested:
 * -
 *
 * Features Tested:
 * -
 *
 * Cases Tested:
 * -
 *
 * Description:
 * -
 *
 * Environment:
 * -
 *
 * Input:
 * - None.
 *
 * Output:
 * - Printed measured elapsed time should closely match specified timeout.
 * - Return code should always be ETIMEDOUT (usually 138 but possibly 10060)
 *
 * Assumptions:
 * -
 *
 * Pass Criteria:
 * - Relies on observation.
 *
 * Fail Criteria:
 * -
 */

#include "test.h"

/*
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/timeb.h>
#include <windows.h>

#include "pthread.h"

#define DEFAULT_MINTIME_INIT    999999999
#define CYG_ONEBILLION          1000000000LL
#define CYG_ONEMILLION          1000000LL
#define CYG_ONEKAPPA            1000LL

#if defined(_MSC_VER) && (_MSC_VER > 1200)
typedef long long cyg_tim_t; //msvc > 6.0
#else
typedef int64_t cyg_tim_t; //msvc 6.0
#endif

LARGE_INTEGER frequency;
LARGE_INTEGER global_start;

cyg_tim_t CYG_DIFFT(cyg_tim_t t1, cyg_tim_t t2)
{
  return (cyg_tim_t)((t2 - t1) * CYG_ONEBILLION / frequency.QuadPart); //nsec
}

void CYG_InitTimers()
{
  QueryPerformanceFrequency(&frequency);
  global_start.QuadPart = 0;
}

void CYG_MARK1(cyg_tim_t *T)
{
  LARGE_INTEGER curTime;
  QueryPerformanceCounter (&curTime);
  *T = (curTime.QuadPart);// + global_start.QuadPart);
}

///////////////////GetTimestampTS/////////////////

#if 1

int GetTimestampTS(struct timespec *tv)
{
  struct _timeb timebuffer;

#if !(_MSC_VER <= 1200)
  _ftime64_s( &timebuffer ); //msvc > 6.0
#else
  _ftime( &timebuffer ); //msvc = 6.0
#endif

  tv->tv_sec = timebuffer.time;
  tv->tv_nsec = 1000000L * timebuffer.millitm;
  return 0;
}

#else

int GetTimestampTS(struct timespec *tv)
{
  static LONGLONG     epoch = 0;
  SYSTEMTIME          local;
  FILETIME            abs;
  LONGLONG            now;

  if(!epoch) {
      memset(&local,0,sizeof(SYSTEMTIME));
      local.wYear = 1970;
      local.wMonth = 1;
      local.wDay = 1;
      local.wHour = 0;
      local.wMinute = 0;
      local.wSecond = 0;
      SystemTimeToFileTime(&local, &abs);
      epoch = *(LONGLONG *)&abs;
  }
  GetSystemTime(&local);
  SystemTimeToFileTime(&local, &abs);
  now = *(LONGLONG *)&abs;
  now = now - epoch;
  tv->tv_sec = (long)(now / 10000000);
  tv->tv_nsec = (long)((now * 100) % 1000000000);

  return 0;
}

#endif

///////////////////GetTimestampTS/////////////////


#define MSEC_F 1000000L
#define USEC_F 1000L
#define NSEC_F 1L

pthread_mutexattr_t mattr_;
pthread_mutex_t mutex_;
pthread_condattr_t cattr_;
pthread_cond_t cv_;

int Init(void)
{
  assert(0 == pthread_mutexattr_init(&mattr_));
  assert(0 == pthread_mutex_init(&mutex_, &mattr_));
  assert(0 == pthread_condattr_init(&cattr_));
  assert(0 == pthread_cond_init(&cv_, &cattr_));
  return 0;
}

int Destroy(void)
{
  assert(0 == pthread_cond_destroy(&cv_));
  assert(0 == pthread_mutex_destroy(&mutex_));
  assert(0 == pthread_mutexattr_destroy(&mattr_));
  assert(0 == pthread_condattr_destroy(&cattr_));
  return 0;
}

int Wait(time_t sec, long nsec)
{
  struct timespec abstime;
  long sc;
  int result = 0;
  GetTimestampTS(&abstime);
  abstime.tv_sec  += sec;
  abstime.tv_nsec += nsec;
  if((sc = (abstime.tv_nsec / 1000000000L))){
      abstime.tv_sec += sc;
      abstime.tv_nsec %= 1000000000L;
  }
  assert(0 == pthread_mutex_lock(&mutex_));
  /*
   * We don't need to check the CV.
   */
  assert(ETIMEDOUT == (result = pthread_cond_timedwait(&cv_, &mutex_, &abstime)));
  assert(0 == pthread_mutex_unlock(&mutex_));
  return result;
}

char tbuf[128];
void printtim(cyg_tim_t rt, cyg_tim_t dt, int wres)
{
  printf("wait result [%d]: timeout(ms) [expected/actual]: %ld/%ld\n", wres, (long)(rt/CYG_ONEMILLION), (long)(dt/CYG_ONEMILLION));
}


int main(int argc, char* argv[])
{
  int i = 0;
  int wres = 0;
  cyg_tim_t t1, t2, dt, rt;

  CYG_InitTimers();

  Init();

  while(i++ < 10){
      rt = 90*i*MSEC_F;
      CYG_MARK1(&t1);
      wres = Wait(0, (long)(size_t)rt);
      CYG_MARK1(&t2);
      dt = CYG_DIFFT(t1, t2);
      printtim(rt, dt, wres);
  }

  Destroy();

  return 0;
}
