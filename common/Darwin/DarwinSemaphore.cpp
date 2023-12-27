// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

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
	if (mach_retval != KERN_SUCCESS)
	{
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

