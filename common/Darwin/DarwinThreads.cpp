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

#include <sched.h>
#include <unistd.h>
#include <mach/mach_init.h>
#include <mach/thread_act.h>
#include <mach/mach_port.h>

#include "common/PrecompiledHeader.h"
#include "common/Threading.h"

// Note: assuming multicore is safer because it forces the interlocked routines to use
// the LOCK prefix.  The prefix works on single core CPUs fine (but is slow), but not
// having the LOCK prefix is very bad indeed.

__forceinline void Threading::Sleep(int ms)
{
	usleep(1000 * ms);
}

__forceinline void Threading::Timeslice()
{
	sched_yield();
}

// For use in spin/wait loops, acts as a hint to Intel CPUs and should, in theory
// improve performance and reduce cpu power consumption.
__forceinline void Threading::SpinWait()
{
	// If this doesn't compile you can just comment it out (it only serves as a
	// performance hint and isn't required).
	__asm__("pause");
}

__forceinline void Threading::EnableHiresScheduler()
{
	// Darwin has customizable schedulers, see xnu/osfmk/man. Not
	// implemented yet though (and not sure if useful for pcsx2).
}

__forceinline void Threading::DisableHiresScheduler()
{
	// see EnableHiresScheduler()
}

// Just like on Windows, this is not really the number of ticks per second,
// but just a factor by which one has to divide GetThreadCpuTime() or
// pxThread::GetCpuTime() if one wants to receive a value in seconds. NOTE:
// doing this will of course yield precision loss.
u64 Threading::GetThreadTicksPerSecond()
{
	return 1000000; // the *CpuTime() functions return values in microseconds
}

// gets the CPU time used by the current thread (both system and user), in
// microseconds, returns 0 on failure
static u64 getthreadtime(thread_port_t thread)
{
	mach_msg_type_number_t count = THREAD_BASIC_INFO_COUNT;
	thread_basic_info_data_t info;

	kern_return_t kr = thread_info(thread, THREAD_BASIC_INFO,
		(thread_info_t)&info, &count);
	if (kr != KERN_SUCCESS)
	{
		return 0;
	}

	// add system and user time
	return (u64)info.user_time.seconds * (u64)1e6 +
		   (u64)info.user_time.microseconds +
		   (u64)info.system_time.seconds * (u64)1e6 +
		   (u64)info.system_time.microseconds;
}

// Returns the current timestamp (not relative to a real world clock) in microseconds
u64 Threading::GetThreadCpuTime()
{
	// we could also use mach_thread_self() and mach_port_deallocate(), but
	// that calls upon mach traps (kinda like system calls). Unless I missed
	// something in the COMMPAGE (like Linux vDSO) which makes overrides it
	// to be user-space instead. In contract,
	// pthread_mach_thread_np(pthread_self()) is entirely in user-space.
	u64 us = getthreadtime(pthread_mach_thread_np(pthread_self()));
	return us;
}

Threading::ThreadHandle::ThreadHandle() = default;

Threading::ThreadHandle::ThreadHandle(const ThreadHandle& handle)
	: m_native_handle(handle.m_native_handle)
{
}

Threading::ThreadHandle::ThreadHandle(ThreadHandle&& handle)
	: m_native_handle(handle.m_native_handle)
{
	handle.m_native_handle = nullptr;
}

Threading::ThreadHandle::~ThreadHandle() = default;

Threading::ThreadHandle Threading::ThreadHandle::GetForCallingThread()
{
	ThreadHandle ret;
	ret.m_native_handle = pthread_self();
	return ret;
}

Threading::ThreadHandle& Threading::ThreadHandle::operator=(ThreadHandle&& handle)
{
	m_native_handle = handle.m_native_handle;
	handle.m_native_handle = nullptr;
	return *this;
}

Threading::ThreadHandle& Threading::ThreadHandle::operator=(const ThreadHandle& handle)
{
	m_native_handle = handle.m_native_handle;
	return *this;
}

u64 Threading::ThreadHandle::GetCPUTime() const
{
	return getthreadtime(pthread_mach_thread_np((pthread_t)m_native_handle));
}

bool Threading::ThreadHandle::SetAffinity(u64 processor_mask) const
{
	// Doesn't appear to be possible to set affinity.
	return false;
}

// name can be up to 16 bytes
void Threading::SetNameOfCurrentThread(const char* name)
{
	pthread_setname_np(name);
}

#endif
