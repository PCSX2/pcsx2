// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "common/Threading.h"
#include "common/Assertions.h"
#include "common/HostSys.h"

#ifdef _WIN32
#include "common/RedtapeWindows.h"
#endif

#include <limits>

// --------------------------------------------------------------------------------------
//  Semaphore Implementations
// --------------------------------------------------------------------------------------

bool Threading::WorkSema::CheckForWork()
{
	s32 value = m_state.load(std::memory_order_relaxed);
	pxAssert(!IsDead(value));

	// we want to switch to the running state, but preserve the waiting empty bit for RUNNING_N -> RUNNING_0
	// otherwise, we clear the waiting flag (since we're notifying the waiter that we're empty below)
	while (!m_state.compare_exchange_weak(value,
		IsReadyForSleep(value) ? STATE_RUNNING_0 : (value & STATE_FLAG_WAITING_EMPTY),
		std::memory_order_acq_rel, std::memory_order_relaxed))
	{
	}

	// if we're not empty, we have work to do
	if (!IsReadyForSleep(value))
		return true;

	// this means we're empty, so notify any waiters
	if (value & STATE_FLAG_WAITING_EMPTY)
		m_empty_sema.Post();

	// no work to do
	return false;
}

void Threading::WorkSema::WaitForWork()
{
	// State change:
	// SLEEPING, SPINNING: This is the worker thread and it's clearly not asleep or spinning, so these states should be impossible
	// RUNNING_0: Change state to SLEEPING, wake up thread if WAITING_EMPTY
	// RUNNING_N: Change state to RUNNING_0 (and preserve WAITING_EMPTY flag)
	s32 value = m_state.load(std::memory_order_relaxed);
	pxAssert(!IsDead(value));
	while (!m_state.compare_exchange_weak(value, NextStateWaitForWork(value), std::memory_order_acq_rel, std::memory_order_relaxed))
		;
	if (IsReadyForSleep(value))
	{
		if (value & STATE_FLAG_WAITING_EMPTY)
			m_empty_sema.Post();
		m_sema.Wait();
		// Acknowledge any additional work added between wake up request and getting here
		m_state.fetch_and(STATE_FLAG_WAITING_EMPTY, std::memory_order_acquire);
	}
}

void Threading::WorkSema::WaitForWorkWithSpin()
{
	s32 value = m_state.load(std::memory_order_relaxed);
	pxAssert(!IsDead(value));
	while (IsReadyForSleep(value))
	{
		if (m_state.compare_exchange_weak(value, STATE_SPINNING, std::memory_order_release, std::memory_order_relaxed))
		{
			if (value & STATE_FLAG_WAITING_EMPTY)
				m_empty_sema.Post();
			value = STATE_SPINNING;
			break;
		}
	}
	u32 waited = 0;
	while (value < 0)
	{
		if (waited > SPIN_TIME_NS)
		{
			if (!m_state.compare_exchange_weak(value, STATE_SLEEPING, std::memory_order_relaxed))
				continue;
			m_sema.Wait();
			break;
		}
		waited += ShortSpin();
		value = m_state.load(std::memory_order_relaxed);
	}
	// Clear back to STATE_RUNNING_0 (but preserve waiting empty flag)
	m_state.fetch_and(STATE_FLAG_WAITING_EMPTY, std::memory_order_acquire);
}

bool Threading::WorkSema::WaitForEmpty()
{
	s32 value = m_state.load(std::memory_order_acquire);
	while (true)
	{
		if (value < 0)
			return !IsDead(value); // STATE_SLEEPING or STATE_SPINNING, queue is empty!
		// Note: We technically only need memory_order_acquire on *failure* (because that's when we could leave without sleeping), but libstdc++ still asserts on failure < success
		if (m_state.compare_exchange_weak(value, value | STATE_FLAG_WAITING_EMPTY, std::memory_order_acquire))
			break;
	}
	pxAssertMsg(!(value & STATE_FLAG_WAITING_EMPTY), "Multiple threads attempted to wait for empty (not currently supported)");
	m_empty_sema.Wait();
	return !IsDead(m_state.load(std::memory_order_relaxed));
}

bool Threading::WorkSema::WaitForEmptyWithSpin()
{
	s32 value = m_state.load(std::memory_order_acquire);
	u32 waited = 0;
	while (true)
	{
		if (value < 0)
			return !IsDead(value); // STATE_SLEEPING or STATE_SPINNING, queue is empty!
		if (waited > SPIN_TIME_NS && m_state.compare_exchange_weak(value, value | STATE_FLAG_WAITING_EMPTY, std::memory_order_acquire))
			break;
		waited += ShortSpin();
		value = m_state.load(std::memory_order_acquire);
	}
	pxAssertMsg(!(value & STATE_FLAG_WAITING_EMPTY), "Multiple threads attempted to wait for empty (not currently supported)");
	m_empty_sema.Wait();
	return !IsDead(m_state.load(std::memory_order_relaxed));
}

void Threading::WorkSema::Kill()
{
	s32 value = m_state.exchange(std::numeric_limits<s32>::min(), std::memory_order_release);
	if (value & STATE_FLAG_WAITING_EMPTY)
		m_empty_sema.Post();
}

void Threading::WorkSema::Reset()
{
	m_state = STATE_RUNNING_0;
}

#if !defined(__APPLE__) // macOS implementations are in DarwinSemaphore

Threading::KernelSemaphore::KernelSemaphore()
{
#ifdef _WIN32
	m_sema = CreateSemaphore(nullptr, 0, LONG_MAX, nullptr);
#else
	sem_init(&m_sema, false, 0);
#endif
}

Threading::KernelSemaphore::~KernelSemaphore()
{
#ifdef _WIN32
	CloseHandle(m_sema);
#else
	sem_destroy(&m_sema);
#endif
}

void Threading::KernelSemaphore::Post()
{
#ifdef _WIN32
	ReleaseSemaphore(m_sema, 1, nullptr);
#else
	sem_post(&m_sema);
#endif
}

void Threading::KernelSemaphore::Wait()
{
#ifdef _WIN32
	WaitForSingleObject(m_sema, INFINITE);
#else
	sem_wait(&m_sema);
#endif
}

bool Threading::KernelSemaphore::TryWait()
{
#ifdef _WIN32
	return WaitForSingleObject(m_sema, 0) == WAIT_OBJECT_0;
#else
	return sem_trywait(&m_sema) == 0;
#endif
}

#endif
