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

#if defined(_WIN32)

#include "common/RedtapeWindows.h"
#include "common/Threading.h"
#include "common/emitter/tools.h"

__fi void Threading::Sleep(int ms)
{
	::Sleep(ms);
}

__fi void Threading::Timeslice()
{
	::Sleep(0);
}

// For use in spin/wait loops,  Acts as a hint to Intel CPUs and should, in theory
// improve performance and reduce cpu power consumption.
__fi void Threading::SpinWait()
{
	_mm_pause();
}

__fi void Threading::EnableHiresScheduler()
{
	// This improves accuracy of Sleep() by some amount, and only adds a negligible amount of
	// overhead on modern CPUs.  Typically desktops are already set pretty low, but laptops in
	// particular may have a scheduler Period of 15 or 20ms to extend battery life.

	// (note: this same trick is used by most multimedia software and games)

	timeBeginPeriod(1);
}

__fi void Threading::DisableHiresScheduler()
{
	timeEndPeriod(1);
}

Threading::ThreadHandle::ThreadHandle() = default;

Threading::ThreadHandle::ThreadHandle(const ThreadHandle& handle)
{
	if (handle.m_native_handle)
	{
		HANDLE new_handle;
		if (DuplicateHandle(GetCurrentProcess(), (HANDLE)handle.m_native_handle,
				GetCurrentProcess(), &new_handle, THREAD_QUERY_INFORMATION | THREAD_SET_LIMITED_INFORMATION, FALSE, 0))
		{
			m_native_handle = (void*)new_handle;
		}
	}
}

Threading::ThreadHandle::ThreadHandle(ThreadHandle&& handle)
	: m_native_handle(handle.m_native_handle)
{
	handle.m_native_handle = nullptr;
}


Threading::ThreadHandle::~ThreadHandle()
{
	if (m_native_handle)
		CloseHandle(m_native_handle);
}

Threading::ThreadHandle Threading::ThreadHandle::GetForCallingThread()
{
	ThreadHandle ret;
	ret.m_native_handle = (void*)OpenThread(THREAD_QUERY_INFORMATION | THREAD_SET_LIMITED_INFORMATION, FALSE, GetCurrentThreadId());
	return ret;
}

Threading::ThreadHandle& Threading::ThreadHandle::operator=(ThreadHandle&& handle)
{
	if (m_native_handle)
		CloseHandle((HANDLE)m_native_handle);
	m_native_handle = handle.m_native_handle;
	handle.m_native_handle = nullptr;
	return *this;
}

Threading::ThreadHandle& Threading::ThreadHandle::operator=(const ThreadHandle& handle)
{
	if (m_native_handle)
	{
		CloseHandle((HANDLE)m_native_handle);
		m_native_handle = nullptr;
	}

	HANDLE new_handle;
	if (DuplicateHandle(GetCurrentProcess(), (HANDLE)handle.m_native_handle,
			GetCurrentProcess(), &new_handle, THREAD_QUERY_INFORMATION | THREAD_SET_LIMITED_INFORMATION, FALSE, 0))
	{
		m_native_handle = (void*)new_handle;
	}

	return *this;
}

u64 Threading::ThreadHandle::GetCPUTime() const
{
	u64 ret = 0;
	if (m_native_handle)
		QueryThreadCycleTime((HANDLE)m_native_handle, &ret);
	return ret;
}

bool Threading::ThreadHandle::SetAffinity(u64 processor_mask) const
{
	if (processor_mask == 0)
		processor_mask = ~processor_mask;

	return (SetThreadAffinityMask(GetCurrentThread(), (DWORD_PTR)processor_mask) != 0 || GetLastError() != ERROR_SUCCESS);
}

u64 Threading::GetThreadCpuTime()
{
	u64 ret = 0;
	QueryThreadCycleTime(GetCurrentThread(), &ret);
	return ret;
}

u64 Threading::GetThreadTicksPerSecond()
{
	// On x86, despite what the MS documentation says, this basically appears to be rdtsc.
	// So, the frequency is our base clock speed (and stable regardless of power management).
	static u64 frequency = 0;
	if (unlikely(frequency == 0))
		frequency = x86caps.CachedMHz() * u64(1000000);
	return frequency;
}

void Threading::SetNameOfCurrentThread(const char* name)
{
	// This feature needs Windows headers and MSVC's SEH support:

#if defined(_WIN32) && defined(_MSC_VER)

	// This code sample was borrowed form some obscure MSDN article.
	// In a rare bout of sanity, it's an actual Microsoft-published hack
	// that actually works!

	static const int MS_VC_EXCEPTION = 0x406D1388;

#pragma pack(push, 8)
	struct THREADNAME_INFO
	{
		DWORD dwType; // Must be 0x1000.
		LPCSTR szName; // Pointer to name (in user addr space).
		DWORD dwThreadID; // Thread ID (-1=caller thread).
		DWORD dwFlags; // Reserved for future use, must be zero.
	};
#pragma pack(pop)

	THREADNAME_INFO info;
	info.dwType = 0x1000;
	info.szName = name;
	info.dwThreadID = GetCurrentThreadId();
	info.dwFlags = 0;

	__try
	{
		RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
	}
#endif
}

#endif
