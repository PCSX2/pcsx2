#include "rc_compat.h"

#include <ctype.h>
#include <stdarg.h>

#ifdef RC_C89_HELPERS

int rc_strncasecmp(const char* left, const char* right, size_t length)
{
  while (length)
  {
    if (*left != *right)
    {
      const int diff = tolower(*left) - tolower(*right);
      if (diff != 0)
        return diff;
    }

    ++left;
    ++right;
    --length;
  }

  return 0;
}

int rc_strcasecmp(const char* left, const char* right)
{
  while (*left || *right)
  {
    if (*left != *right)
    {
      const int diff = tolower(*left) - tolower(*right);
      if (diff != 0)
        return diff;
    }

    ++left;
    ++right;
  }

  return 0;
}

char* rc_strdup(const char* str)
{
  const size_t length = strlen(str);
  char* buffer = (char*)malloc(length + 1);
  if (buffer)
    memcpy(buffer, str, length + 1);
  return buffer;
}

int rc_snprintf(char* buffer, size_t size, const char* format, ...)
{
  int result;
  va_list args;

  va_start(args, format);

#ifdef __STDC_WANT_SECURE_LIB__
  result = vsprintf_s(buffer, size, format, args);
#else
  /* assume buffer is large enough and ignore size */
  (void)size;
  result = vsprintf(buffer, format, args);
#endif

  va_end(args);

  return result;
}

#endif

#ifndef __STDC_WANT_SECURE_LIB__

struct tm* rc_gmtime_s(struct tm* buf, const time_t* timer)
{
  struct tm* tm = gmtime(timer);
  memcpy(buf, tm, sizeof(*tm));
  return buf;
}

#endif

#ifndef RC_NO_THREADS

#if defined(_WIN32)

/* https://gist.github.com/roxlu/1c1af99f92bafff9d8d9 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
 
void rc_mutex_init(rc_mutex_t* mutex)
{
  /* default security, not owned by calling thread, unnamed */
  mutex->handle = CreateMutex(NULL, FALSE, NULL);
}

void rc_mutex_destroy(rc_mutex_t* mutex)
{
  CloseHandle(mutex->handle);
}

void rc_mutex_lock(rc_mutex_t* mutex)
{
  WaitForSingleObject(mutex->handle, 0xFFFFFFFF);
}

void rc_mutex_unlock(rc_mutex_t* mutex)
{
  ReleaseMutex(mutex->handle);
}

#elif defined(GEKKO)

/* https://github.com/libretro/RetroArch/pull/16116 */

void rc_mutex_init(rc_mutex_t* mutex)
{
  LWP_MutexInit(mutex, NULL);
}

void rc_mutex_destroy(rc_mutex_t* mutex)
{
  LWP_MutexDestroy(mutex);
}

void rc_mutex_lock(rc_mutex_t* mutex)
{
  LWP_MutexLock(mutex);
}

void rc_mutex_unlock(rc_mutex_t* mutex)
{
  LWP_MutexUnlock(mutex);
}

#else

void rc_mutex_init(rc_mutex_t* mutex)
{
  pthread_mutex_init(mutex, NULL);
}

void rc_mutex_destroy(rc_mutex_t* mutex)
{
  pthread_mutex_destroy(mutex);
}

void rc_mutex_lock(rc_mutex_t* mutex)
{
  pthread_mutex_lock(mutex);
}

void rc_mutex_unlock(rc_mutex_t* mutex)
{
  pthread_mutex_unlock(mutex);
}

#endif
#endif /* RC_NO_THREADS */
