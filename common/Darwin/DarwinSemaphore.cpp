/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2014  PCSX2 Dev Team
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

#if defined(__APPLE__)

#include <cstdio>
#include <cassert> // assert
#include <pthread.h> // pthread_setcancelstate()
#include <sys/time.h> // gettimeofday()
#include <mach/mach.h>
#include <mach/task.h> // semaphore_create() and semaphore_destroy()
#include <mach/semaphore.h> // semaphore_*()
#include <mach/mach_error.h> // mach_error_string()
#include <mach/mach_time.h> // mach_absolute_time()

#include "common/Threading.h"
#include "common/ThreadingInternal.h"
#include "common/wxBaseTools.h"

// --------------------------------------------------------------------------------------
//  Semaphore Implementation for Darwin/OSX
//
//  Sadly, Darwin/OSX needs its own implementation of Semaphores instead of
//  relying on phtreads, because OSX unnamed semaphore (the best kind)
//  support is very poor.
//
//  This implementation makes use of Mach primitives instead. These are also
//  what Grand Central Dispatch (GCD) is based on, as far as I understand:
//  http://newosxbook.com/articles/GCD.html.
//
// --------------------------------------------------------------------------------------

static void MACH_CHECK(kern_return_t mach_retval)
{
	switch (mach_retval)
	{
		case KERN_SUCCESS:
			break;
		case KERN_ABORTED: // Awoken due reason unrelated to semaphore (e.g. pthread_cancel)
			pthread_testcancel(); // Unlike sem_wait, mach semaphore ops aren't cancellation points
			// fallthrough
		default:
			fprintf(stderr, "mach error: %s", mach_error_string(mach_retval));
			assert(mach_retval == KERN_SUCCESS);
	}
}

Threading::Semaphore::Semaphore()
{
	// other platforms explicitly make a thread-private (unshared) semaphore
	// here. But it seems Mach doesn't support that.
	MACH_CHECK(semaphore_create(mach_task_self(), (semaphore_t*)&m_sema, SYNC_POLICY_FIFO, 0));
	__atomic_store_n(&m_counter, 0, __ATOMIC_RELEASE);
}

Threading::Semaphore::~Semaphore()
{
	MACH_CHECK(semaphore_destroy(mach_task_self(), (semaphore_t)m_sema));
}

void Threading::Semaphore::Reset()
{
	MACH_CHECK(semaphore_destroy(mach_task_self(), (semaphore_t)m_sema));
	MACH_CHECK(semaphore_create(mach_task_self(), (semaphore_t*)&m_sema, SYNC_POLICY_FIFO, 0));
	__atomic_store_n(&m_counter, 0, __ATOMIC_SEQ_CST);
}

void Threading::Semaphore::Post()
{
	if (__atomic_fetch_add(&m_counter, 1, __ATOMIC_RELEASE) < 0)
		MACH_CHECK(semaphore_signal(m_sema));
}

void Threading::Semaphore::Post(int multiple)
{
	for (int i = 0; i < multiple; ++i)
	{
		Post();
	}
}

void Threading::Semaphore::WaitWithoutYield()
{
	pxAssertMsg(!wxThread::IsMain(), "Unyielding semaphore wait issued from the main/gui thread.  Please use Wait() instead.");
	if (__atomic_sub_fetch(&m_counter, 1, __ATOMIC_ACQUIRE) < 0)
		MACH_CHECK(semaphore_wait(m_sema));
}

bool Threading::Semaphore::WaitWithoutYield(const wxTimeSpan& timeout)
{
	// This method is the reason why there has to be a special Darwin
	// implementation of Semaphore. Note that semaphore_timedwait() is prone
	// to returning with KERN_ABORTED, which basically signifies that some
	// signal has worken it up. The best official "documentation" for
	// semaphore_timedwait() is the way it's used in Grand Central Dispatch,
	// which is open-source.

	if (__atomic_sub_fetch(&m_counter, 1, __ATOMIC_ACQUIRE) >= 0)
		return true;

	// on x86 platforms, mach_absolute_time() returns nanoseconds
	// TODO(aktau): on iOS a scale value from mach_timebase_info will be necessary
	u64 const kOneThousand = 1000;
	u64 const kOneBillion = kOneThousand * kOneThousand * kOneThousand;
	u64 const delta = timeout.GetMilliseconds().GetValue() * (kOneThousand * kOneThousand);
	mach_timespec_t ts;
	kern_return_t kr = KERN_ABORTED;
	for (u64 now = mach_absolute_time(), deadline = now + delta;
		 kr == KERN_ABORTED; now = mach_absolute_time())
	{
		if (now > deadline)
		{
			// timed out by definition
			kr = KERN_OPERATION_TIMED_OUT;
			break;
		}

		u64 timeleft = deadline - now;
		ts.tv_sec = timeleft / kOneBillion;
		ts.tv_nsec = timeleft % kOneBillion;

		// possible return values of semaphore_timedwait() (from XNU sources):
		// internal kernel val -> return value
		// THREAD_INTERRUPTED  -> KERN_ABORTED
		// THREAD_TIMED_OUT    -> KERN_OPERATION_TIMED_OUT
		// THREAD_AWAKENED     -> KERN_SUCCESS
		// THREAD_RESTART      -> KERN_TERMINATED
		// default             -> KERN_FAILURE
		kr = semaphore_timedwait(m_sema, ts);
	}

	if (kr == KERN_OPERATION_TIMED_OUT)
	{
		int orig = __atomic_load_n(&m_counter, __ATOMIC_RELAXED);
		while (orig < 0)
		{
			if (__atomic_compare_exchange_n(&m_counter, &orig, orig + 1, true, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED))
				return false;
		}
		// Semaphore was signalled between our wait expiring and now, keep kernel sema in sync
		kr = semaphore_wait(m_sema);
	}

	// while it's entirely possible to have KERN_FAILURE here, we should
	// probably assert so we can study and correct the actual error here
	// (the thread dying while someone is wainting for it).
	MACH_CHECK(kr);
	return true;
}

// This is a wxApp-safe implementation of Wait, which makes sure and executes the App's
// pending messages *if* the Wait is performed on the Main/GUI thread. This ensures that
// user input continues to be handled and that windows continue to repaint. If the Wait is
// called from another thread, no message pumping is performed.
void Threading::Semaphore::Wait()
{
#if wxUSE_GUI
	if (!wxThread::IsMain() || (wxTheApp == NULL))
	{
		WaitWithoutYield();
	}
	else if (_WaitGui_RecursionGuard(L"Semaphore::Wait"))
	{
		WaitWithoutYield();
	}
	else
	{
		while (!WaitWithoutYield(def_yieldgui_interval))
		{
			YieldToMain();
		}
	}
#else
	WaitWithoutYield();
#endif
}

// This is a wxApp-safe implementation of WaitWithoutYield, which makes sure and executes the App's
// pending messages *if* the Wait is performed on the Main/GUI thread.  This ensures that
// user input continues to be handled and that windows continue to repaint.  If the Wait is
// called from another thread, no message pumping is performed.
//
// Returns:
//   false if the wait timed out before the semaphore was signaled, or true if the signal was
//   reached prior to timeout.
//
bool Threading::Semaphore::Wait(const wxTimeSpan& timeout)
{
#if wxUSE_GUI
	if (!wxThread::IsMain() || (wxTheApp == NULL))
	{
		return WaitWithoutYield(timeout);
	}
	else if (_WaitGui_RecursionGuard(L"Semaphore::TimedWait"))
	{
		return WaitWithoutYield(timeout);
	}
	else
	{
		wxTimeSpan countdown((timeout));

		do
		{
			if (WaitWithoutYield(def_yieldgui_interval))
				break;
			YieldToMain();
			countdown -= def_yieldgui_interval;
		} while (countdown.GetMilliseconds() > 0);

		return countdown.GetMilliseconds() > 0;
	}
#else
	return WaitWithoutYield(timeout);
#endif
}

bool Threading::Semaphore::TryWait()
{
	int counter = __atomic_load_n(&m_counter, __ATOMIC_RELAXED);
	while (counter > 0 && !__atomic_compare_exchange_n(&m_counter, &counter, counter - 1, true, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED))
		;
	return counter > 0;
}

// Performs an uncancellable wait on a semaphore; restoring the thread's previous cancel state
// after the wait has completed.  Useful for situations where the semaphore itself is stored on
// the stack and passed to another thread via GUI message or such, avoiding complications where
// the thread might be canceled and the stack value becomes invalid.
//
// Performance note: this function has quite a bit more overhead compared to Semaphore::WaitWithoutYield(), so
// consider manually specifying the thread as uncancellable and using WaitWithoutYield() instead if you need
// to do a lot of no-cancel waits in a tight loop worker thread, for example.
//
// I'm unsure how to do this with pure Mach primitives, the docs in
// osfmk/man seem a bit out of date so perhaps there's a possibility, but
// since as far as I know Mach threads are 1-to-1 on BSD uthreads (and thus
// POSIX threads), this should work. -- aktau
void Threading::Semaphore::WaitNoCancel()
{
	int oldstate;
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);
	Wait();
	pthread_setcancelstate(oldstate, NULL);
}

void Threading::Semaphore::WaitNoCancel(const wxTimeSpan& timeout)
{
	int oldstate;
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);
	Wait(timeout);
	pthread_setcancelstate(oldstate, NULL);
}

int Threading::Semaphore::Count()
{
	return __atomic_load_n(&m_counter, __ATOMIC_RELAXED);
}
#endif
