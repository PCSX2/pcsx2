/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "PrecompiledHeader.h"
#include "PersistentThread.h"
#include <wx/thread.h>

namespace Threading
{
	static std::atomic<int> _attr_refcount(0);
	static pthread_mutexattr_t _attr_recursive;
} // namespace Threading

// --------------------------------------------------------------------------------------
//  Mutex Implementations
// --------------------------------------------------------------------------------------

#if defined(_WIN32) || (defined(_POSIX_TIMEOUTS) && _POSIX_TIMEOUTS >= 200112L)
// good, we have pthread_mutex_timedlock
#define xpthread_mutex_timedlock pthread_mutex_timedlock
#else
// We have to emulate pthread_mutex_timedlock(). This could be a serious
// performance drain if its used a lot.

#include <sys/time.h> // gettimeofday()

// sleep for 10ms at a time
#define TIMEDLOCK_EMU_SLEEP_NS 10000000ULL

// Original POSIX docs:
//
// The pthread_mutex_timedlock() function shall lock the mutex object
// referenced by mutex. If the mutex is already locked, the calling thread
// shall block until the mutex becomes available as in the
// pthread_mutex_lock() function. If the mutex cannot be locked without
// waiting for another thread to unlock the mutex, this wait shall be
// terminated when the specified timeout expires.
//
// This is an implementation that emulates pthread_mutex_timedlock() via
// pthread_mutex_trylock().
static int xpthread_mutex_timedlock(
	pthread_mutex_t* mutex,
	const struct timespec* abs_timeout)
{
	int err = 0;

	while ((err = pthread_mutex_trylock(mutex)) == EBUSY)
	{
		// check if the timeout has expired, gettimeofday() is implemented
		// efficiently (in userspace) on OSX
		struct timeval now;
		gettimeofday(&now, NULL);
		if (now.tv_sec > abs_timeout->tv_sec || (now.tv_sec == abs_timeout->tv_sec && (u64)now.tv_usec * 1000ULL > (u64)abs_timeout->tv_nsec))
		{
			return ETIMEDOUT;
		}

		// acquiring lock failed, sleep some
		struct timespec ts;
		ts.tv_sec = 0;
		ts.tv_nsec = TIMEDLOCK_EMU_SLEEP_NS;
		while (nanosleep(&ts, &ts) == -1)
			;
	}

	return err;
}
#endif

Threading::Mutex::Mutex()
{
	pthread_mutex_init(&m_mutex, NULL);
}

static wxTimeSpan def_detach_timeout(0, 0, 6, 0);

void Threading::Mutex::Detach()
{
	if (EBUSY != pthread_mutex_destroy(&m_mutex))
		return;

	if (IsRecursive())
	{
		// Sanity check: Recursive locks could be held by our own thread, which would
		// be considered an assertion failure, but can also be handled gracefully.
		// (note: if the mutex is locked recursively more than twice then this assert won't
		//  detect it)

		Release();
		Release(); // in case of double recursion.
		int result = pthread_mutex_destroy(&m_mutex);
		if (pxAssertDev(result != EBUSY, "Detachment of a recursively-locked mutex (self-locked!)."))
			return;
	}

	if (Wait(def_detach_timeout))
		pthread_mutex_destroy(&m_mutex);
	else
		Console.Error("(Thread Log) Mutex cleanup failed due to possible deadlock.");
}

Threading::Mutex::~Mutex()
{
	try
	{
		Mutex::Detach();
	}
	DESTRUCTOR_CATCHALL;
}

Threading::MutexRecursive::MutexRecursive()
	: Mutex(false)
{
	if (++_attr_refcount == 1)
	{
		if (0 != pthread_mutexattr_init(&_attr_recursive))
			throw Exception::OutOfMemory("Recursive mutexing attributes");

		pthread_mutexattr_settype(&_attr_recursive, PTHREAD_MUTEX_RECURSIVE);
	}

	if (pthread_mutex_init(&m_mutex, &_attr_recursive))
		Console.Error("(Thread Log) Failed to initialize mutex.");
}

Threading::MutexRecursive::~MutexRecursive()
{
	if (--_attr_refcount == 0)
		pthread_mutexattr_destroy(&_attr_recursive);
}

// This is a bit of a hackish function, which is technically unsafe, but can be useful for allowing
// the application to survive unexpected or inconvenient failures, where a mutex is deadlocked by
// a rogue thread.  This function allows us to Recreate the mutex and let the deadlocked one ponder
// the deeper meanings of the universe for eternity.
void Threading::Mutex::Recreate()
{
	Detach();
	pthread_mutex_init(&m_mutex, NULL);
}

// Returns:
//   true if the mutex had to be recreated due to lock contention, or false if the mutex is safely
//   unlocked.
bool Threading::Mutex::RecreateIfLocked()
{
	if (!Wait(def_detach_timeout))
	{
		Recreate();
		return true;
	}
	return false;
}


// This is a direct blocking action -- very fast, very efficient, and generally very dangerous
// if used from the main GUI thread, since it typically results in an unresponsive program.
// Call this method directly only if you know the code in question will be run from threads
// other than the main thread.
void Threading::Mutex::AcquireWithoutYield()
{
	pxAssertMsg(!wxThread::IsMain(), "Unyielding mutex acquire issued from the main/gui thread.  Please use Acquire() instead.");
	pthread_mutex_lock(&m_mutex);
}

bool Threading::Mutex::AcquireWithoutYield(const wxTimeSpan& timeout)
{
	wxDateTime megafail(wxDateTime::UNow() + timeout);
	const timespec fail = {megafail.GetTicks(), megafail.GetMillisecond() * 1000000};
	return xpthread_mutex_timedlock(&m_mutex, &fail) == 0;
}

void Threading::Mutex::Release()
{
	pthread_mutex_unlock(&m_mutex);
}

bool Threading::Mutex::TryAcquire()
{
	return EBUSY != pthread_mutex_trylock(&m_mutex);
}

// This is a wxApp-safe rendition of AcquireWithoutYield, which makes sure to execute pending app events
// and messages *if* the lock is performed from the main GUI thread.
//
void Threading::Mutex::Acquire()
{
	pthread_mutex_lock(&m_mutex);
}

bool Threading::Mutex::Acquire(const wxTimeSpan& timeout)
{
	return AcquireWithoutYield(timeout);
}

// Performs a wait on a locked mutex, or returns instantly if the mutex is unlocked.
// Typically this action is used to determine if a thread is currently performing some
// specific task, and to block until the task is finished (PersistentThread uses it to
// determine if the thread is running or completed, for example).
//
// Implemented internally as a simple Acquire/Release pair.
//
void Threading::Mutex::Wait()
{
	Acquire();
	Release();
}

// Like wait but spins for a while before sleeping the thread
void Threading::Mutex::WaitWithSpin()
{
	u32 waited = 0;
	while (true)
	{
		if (TryAcquire())
		{
			Release();
			return;
		}
		if (waited >= SPIN_TIME_NS)
			break;
		waited += ShortSpin();
	}
	Wait();
}

void Threading::Mutex::WaitWithoutYield()
{
	AcquireWithoutYield();
	Release();
}

// Performs a wait on a locked mutex, or returns instantly if the mutex is unlocked.
// (Implemented internally as a simple Acquire/Release pair.)
//
// Returns:
//   true if the mutex was freed and is in an unlocked state; or false if the wait timed out
//   and the mutex is still locked by another thread.
//
bool Threading::Mutex::Wait(const wxTimeSpan& timeout)
{
	if (Acquire(timeout))
	{
		Release();
		return true;
	}
	return false;
}

bool Threading::Mutex::WaitWithoutYield(const wxTimeSpan& timeout)
{
	if (AcquireWithoutYield(timeout))
	{
		Release();
		return true;
	}
	return false;
}

// --------------------------------------------------------------------------------------
//  ScopedLock Implementations
// --------------------------------------------------------------------------------------

Threading::ScopedLock::~ScopedLock()
{
	if (m_IsLocked && m_lock)
		m_lock->Release();
}

Threading::ScopedLock::ScopedLock(const Mutex* locker)
{
	m_IsLocked = false;
	AssignAndLock(locker);
}

Threading::ScopedLock::ScopedLock(const Mutex& locker)
{
	m_IsLocked = false;
	AssignAndLock(locker);
}

void Threading::ScopedLock::AssignAndLock(const Mutex& locker)
{
	AssignAndLock(&locker);
}

void Threading::ScopedLock::AssignAndLock(const Mutex* locker)
{
	pxAssert(!m_IsLocked); // if we're already locked, changing the lock is bad mojo.

	m_lock = const_cast<Mutex*>(locker);
	if (!m_lock)
		return;

	m_IsLocked = true;
	m_lock->Acquire();
}

void Threading::ScopedLock::Assign(const Mutex& locker)
{
	m_lock = const_cast<Mutex*>(&locker);
}

void Threading::ScopedLock::Assign(const Mutex* locker)
{
	m_lock = const_cast<Mutex*>(locker);
}

// Provides manual unlocking of a scoped lock prior to object destruction.
void Threading::ScopedLock::Release()
{
	if (!m_IsLocked)
		return;
	m_IsLocked = false;
	if (m_lock)
		m_lock->Release();
}

// provides manual locking of a scoped lock, to re-lock after a manual unlocking.
void Threading::ScopedLock::Acquire()
{
	if (m_IsLocked || !m_lock)
		return;
	m_lock->Acquire();
	m_IsLocked = true;
}

Threading::ScopedLock::ScopedLock(const Mutex& locker, bool isTryLock)
{
	m_lock = const_cast<Mutex*>(&locker);
	if (!m_lock)
		return;
	m_IsLocked = isTryLock ? m_lock->TryAcquire() : false;
}
