/*
 * Copyright © 2016 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */

#if !defined(CUBEB_UTILS_UNIX)
#define CUBEB_UTILS_UNIX

#include <errno.h>
#include <pthread.h>
#include <stdio.h>

/* This wraps a critical section to track the owner in debug mode. */
class owned_critical_section {
public:
  owned_critical_section()
  {
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
#ifndef NDEBUG
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
#else
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_NORMAL);
#endif

#ifndef NDEBUG
    int r =
#endif
        pthread_mutex_init(&mutex, &attr);
#ifndef NDEBUG
    assert(r == 0);
#endif

    pthread_mutexattr_destroy(&attr);
  }

  ~owned_critical_section()
  {
#ifndef NDEBUG
    int r =
#endif
        pthread_mutex_destroy(&mutex);
#ifndef NDEBUG
    assert(r == 0);
#endif
  }

  void lock()
  {
#ifndef NDEBUG
    int r =
#endif
        pthread_mutex_lock(&mutex);
#ifndef NDEBUG
    assert(r == 0 && "Deadlock");
#endif
  }

  void unlock()
  {
#ifndef NDEBUG
    int r =
#endif
        pthread_mutex_unlock(&mutex);
#ifndef NDEBUG
    assert(r == 0 && "Unlocking unlocked mutex");
#endif
  }

  void assert_current_thread_owns()
  {
#ifndef NDEBUG
    int r = pthread_mutex_lock(&mutex);
    assert(r == EDEADLK);
#endif
  }

private:
  pthread_mutex_t mutex;

  // Disallow copy and assignment because pthread_mutex_t cannot be copied.
  owned_critical_section(const owned_critical_section &);
  owned_critical_section & operator=(const owned_critical_section &);
};

#endif /* CUBEB_UTILS_UNIX */
