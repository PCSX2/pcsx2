/*
 * Copyright © 2016 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */

#if !defined(CUBEB_UTILS_WIN)
#define CUBEB_UTILS_WIN

#include "cubeb-internal.h"
#include <windows.h>

/* This wraps an SRWLock to track the owner in debug mode, adapted from
   NSPR and http://blogs.msdn.com/b/oldnewthing/archive/2013/07/12/10433554.aspx
 */
class owned_critical_section {
public:
  owned_critical_section()
      : srwlock(SRWLOCK_INIT)
#ifndef NDEBUG
        ,
        owner(0)
#endif
  {
  }

  void lock()
  {
    AcquireSRWLockExclusive(&srwlock);
#ifndef NDEBUG
    XASSERT(owner != GetCurrentThreadId() && "recursive locking");
    owner = GetCurrentThreadId();
#endif
  }

  void unlock()
  {
#ifndef NDEBUG
    /* GetCurrentThreadId cannot return 0: it is not a the valid thread id */
    owner = 0;
#endif
    ReleaseSRWLockExclusive(&srwlock);
  }

  /* This is guaranteed to have the good behaviour if it succeeds. The behaviour
     is undefined otherwise. */
  void assert_current_thread_owns()
  {
#ifndef NDEBUG
    /* This implies owner != 0, because GetCurrentThreadId cannot return 0. */
    XASSERT(owner == GetCurrentThreadId());
#endif
  }

private:
  SRWLOCK srwlock;
#ifndef NDEBUG
  DWORD owner;
#endif

  // Disallow copy and assignment because SRWLock cannot be copied.
  owned_critical_section(const owned_critical_section &);
  owned_critical_section & operator=(const owned_critical_section &);
};

#endif /* CUBEB_UTILS_WIN */
