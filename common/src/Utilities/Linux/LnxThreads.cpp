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


#include "../PrecompiledHeader.h"
#include "PersistentThread.h"
#include <unistd.h>
#if defined(__linux__)
#include <sys/prctl.h>
#elif defined(__unix__)
#include <pthread_np.h>
#endif

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
    if (id) {
        int err = pthread_getcpuclockid((pthread_t)id, &cid);
        if (err)
            return 0;
    } else {
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

u64 Threading::pxThread::GetCpuTime() const
{
    // Get the cpu time for the thread belonging to this object.  Use m_native_id and/or
    // m_native_handle to implement it. Return value should be a measure of total time the
    // thread has used on the CPU (scaled by the value returned by GetThreadTicksPerSecond(),
    // which typically would be an OS-provided scalar or some sort).

    if (!m_native_id)
        return 0;

    return get_thread_time(m_native_id);
}

void Threading::pxThread::_platform_specific_OnStartInThread()
{
    // Obtain linux-specific thread IDs or Handles here, which can be used to query
    // kernel scheduler performance information.
    m_native_id = (uptr)pthread_self();
}

void Threading::pxThread::_platform_specific_OnCleanupInThread()
{
    // Cleanup handles here, which were opened above.
}

void Threading::SetNameOfCurrentThread(const char *name)
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
