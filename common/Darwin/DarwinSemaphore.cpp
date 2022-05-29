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

Threading::KernelSemaphore::KernelSemaphore()
{
	MACH_CHECK(semaphore_create(mach_task_self(), &m_sema, SYNC_POLICY_FIFO, 0));
}

Threading::KernelSemaphore::~KernelSemaphore()
{
	MACH_CHECK(semaphore_destroy(mach_task_self(), m_sema));
}

void Threading::KernelSemaphore::Post()
{
	MACH_CHECK(semaphore_signal(m_sema));
}

void Threading::KernelSemaphore::Wait()
{
	MACH_CHECK(semaphore_wait(m_sema));
}

bool Threading::KernelSemaphore::TryWait()
{
	mach_timespec_t time = {};
	kern_return_t res = semaphore_timedwait(m_sema, time);
	if (res == KERN_OPERATION_TIMED_OUT)
		return false;
	MACH_CHECK(res);
	return true;
}

#endif

