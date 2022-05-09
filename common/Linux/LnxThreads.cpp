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

#if !defined(_WIN32) && !defined(__APPLE__)
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <unistd.h>
#if defined(__linux__)
#include <sys/prctl.h>
#include <sys/types.h>
#include <sched.h>

// glibc < v2.30 doesn't define gettid...
#if __GLIBC__ == 2 && __GLIBC_MINOR__ < 30
#include <sys/syscall.h>
#define gettid() syscall(SYS_gettid)
#endif

#elif defined(__unix__)
#include <pthread_np.h>
#endif

#include "common/Threading.h"

// We wont need this until we actually have this more then just stubbed out, so I'm commenting this out
// to remove an unneeded dependency.
//#include "x86emitter/tools.h"

#if !defined(__unix__)

#pragma message("LnxThreads.cpp should only be compiled by projects or makefiles targeted at Linux/BSD distros.")

#else

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

// For use in spin/wait loops,  Acts as a hint to Intel CPUs and should, in theory
// improve performance and reduce cpu power consumption.
__forceinline void Threading::SpinWait()
{
	// If this doesn't compile you can just comment it out (it only serves as a
	// performance hint and isn't required).
	__asm__("pause");
}

__forceinline void Threading::EnableHiresScheduler()
{
	// Don't know if linux has a customizable scheduler resolution like Windows (doubtful)
}

__forceinline void Threading::DisableHiresScheduler()
{
}

// Unit of time of GetThreadCpuTime/GetCpuTime
u64 Threading::GetThreadTicksPerSecond()
{
	return 1000000;
}

// Helper function to get either either the current cpu usage
// in called thread or in id thread
static u64 get_thread_time(uptr id = 0)
{
	clockid_t cid;
	if (id)
	{
		int err = pthread_getcpuclockid((pthread_t)id, &cid);
		if (err)
			return 0;
	}
	else
	{
		cid = CLOCK_THREAD_CPUTIME_ID;
	}

	struct timespec ts;
	int err = clock_gettime(cid, &ts);
	if (err)
		return 0;

	return (u64)ts.tv_sec * (u64)1e6 + (u64)ts.tv_nsec / (u64)1e3;
}

// Returns the current timestamp (not relative to a real world clock)
u64 Threading::GetThreadCpuTime()
{
	return get_thread_time();
}

Threading::ThreadHandle::ThreadHandle() = default;

Threading::ThreadHandle::ThreadHandle(const ThreadHandle& handle)
	: m_native_handle(handle.m_native_handle)
#ifdef __linux__
	, m_native_id(handle.m_native_id)
#endif
{
}

Threading::ThreadHandle::ThreadHandle(ThreadHandle&& handle)
	: m_native_handle(handle.m_native_handle)
#ifdef __linux__
	, m_native_id(handle.m_native_id)
#endif
{
	handle.m_native_handle = nullptr;
#ifdef __linux__
	handle.m_native_id = 0;
#endif
}

Threading::ThreadHandle::~ThreadHandle() = default;

Threading::ThreadHandle Threading::ThreadHandle::GetForCallingThread()
{
	ThreadHandle ret;
	ret.m_native_handle = (void*)pthread_self();
#ifdef __linux__
	ret.m_native_id = gettid();
#endif
	return ret;
}

Threading::ThreadHandle& Threading::ThreadHandle::operator=(ThreadHandle&& handle)
{
	m_native_handle = handle.m_native_handle;
	handle.m_native_handle = nullptr;
#ifdef __linux__
	m_native_id = handle.m_native_id;
	handle.m_native_id = 0;
#endif
	return *this;
}

Threading::ThreadHandle& Threading::ThreadHandle::operator=(const ThreadHandle& handle)
{
	m_native_handle = handle.m_native_handle;
#ifdef __linux__
	m_native_id = handle.m_native_id;
#endif
	return *this;
}

u64 Threading::ThreadHandle::GetCPUTime() const
{
	return m_native_handle ? get_thread_time((uptr)m_native_handle) : 0;
}

bool Threading::ThreadHandle::SetAffinity(u64 processor_mask) const
{
#if defined(__linux__)
	cpu_set_t set;
	CPU_ZERO(&set);

	if (processor_mask != 0)
	{
		for (u32 i = 0; i < 64; i++)
		{
			if (processor_mask & (static_cast<u64>(1) << i))
			{
				CPU_SET(i, &set);
			}
		}
	}
	else
	{
		long num_processors = sysconf(_SC_NPROCESSORS_CONF);
		for (long i = 0; i < num_processors; i++)
		{
			CPU_SET(i, &set);
		}
	}

	return sched_setaffinity((pid_t)m_native_id, sizeof(set), &set) >= 0;
#else
	return false;
#endif
}

void Threading::SetNameOfCurrentThread(const char* name)
{
#if defined(__linux__)
	// Extract of manpage: "The name can be up to 16 bytes long, and should be
	//						null-terminated if it contains fewer bytes."
	prctl(PR_SET_NAME, name, 0, 0, 0);
#elif defined(__unix__)
	pthread_set_name_np(pthread_self(), name);
#endif
}

#endif
#endif
