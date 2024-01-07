// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#if defined(_WIN32)

#include "common/Threading.h"
#include "common/Assertions.h"
#include "common/RedtapeWindows.h"

#include <memory>

#include <mmsystem.h>
#include <process.h>
#include <timeapi.h>

__fi void Threading::Timeslice()
{
	::Sleep(0);
}

// For use in spin/wait loops,  Acts as a hint to Intel CPUs and should, in theory
// improve performance and reduce cpu power consumption.
__fi void Threading::SpinWait()
{
#ifdef _M_X86
	_mm_pause();
#else
	YieldProcessor();
#endif
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

Threading::Thread::Thread() = default;

Threading::Thread::Thread(Thread&& thread)
	: ThreadHandle(thread)
	, m_stack_size(thread.m_stack_size)
{
	thread.m_stack_size = 0;
}

Threading::Thread::Thread(EntryPoint func)
	: ThreadHandle()
{
	if (!Start(std::move(func)))
		pxFailRel("Failed to start implicitly started thread.");
}

Threading::Thread::~Thread()
{
	pxAssertRel(!m_native_handle, "Thread should be detached or joined at destruction");
}

void Threading::Thread::SetStackSize(u32 size)
{
	pxAssertRel(!m_native_handle, "Can't change the stack size on a started thread");
	m_stack_size = size;
}

unsigned Threading::Thread::ThreadProc(void* param)
{
	std::unique_ptr<EntryPoint> entry(static_cast<EntryPoint*>(param));
	(*entry.get())();
	return 0;
}

bool Threading::Thread::Start(EntryPoint func)
{
	pxAssertRel(!m_native_handle, "Can't start an already-started thread");
	
	std::unique_ptr<EntryPoint> func_clone(std::make_unique<EntryPoint>(std::move(func)));
	unsigned thread_id;
	m_native_handle = reinterpret_cast<void*>(_beginthreadex(nullptr, m_stack_size, ThreadProc, func_clone.get(), 0, &thread_id));
	if (!m_native_handle)
		return false;

	// thread started, it'll release the memory
	func_clone.release();
	return true;
}

void Threading::Thread::Detach()
{
	pxAssertRel(m_native_handle, "Can't detach without a thread");
	CloseHandle((HANDLE)m_native_handle);
	m_native_handle = nullptr;
}

void Threading::Thread::Join()
{
	pxAssertRel(m_native_handle, "Can't join without a thread");
	const DWORD res = WaitForSingleObject((HANDLE)m_native_handle, INFINITE);
	if (res != WAIT_OBJECT_0)
		pxFailRel("WaitForSingleObject() for thread join failed");

	CloseHandle((HANDLE)m_native_handle);
	m_native_handle = nullptr;
}

Threading::ThreadHandle& Threading::Thread::operator=(Thread&& thread)
{
	ThreadHandle::operator=(thread);
	m_stack_size = thread.m_stack_size;
	thread.m_stack_size = 0;
	return *this;
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
	if (frequency == 0) [[unlikely]]
	{
		HKEY key;
		LSTATUS res =
			RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &key);
		if (res == ERROR_SUCCESS)
		{
			DWORD mhz;
			DWORD size = sizeof(mhz);
			res = RegQueryValueExW(key, L"~MHz", nullptr, nullptr, reinterpret_cast<LPBYTE>(&mhz), &size);
			if (res == ERROR_SUCCESS)
				frequency = static_cast<u64>(mhz) * static_cast<u64>(1000000);
			RegCloseKey(key);
		}
	}
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
