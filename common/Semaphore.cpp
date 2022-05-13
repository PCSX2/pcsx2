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

#include "common/Threading.h"
#include "common/Assertions.h"

#ifdef _WIN32
#include "common/RedtapeWindows.h"
#endif

#include <limits>

// --------------------------------------------------------------------------------------
//  Semaphore Implementations
// --------------------------------------------------------------------------------------


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
	pxAssertDev(!(value & STATE_FLAG_WAITING_EMPTY), "Multiple threads attempted to wait for empty (not currently supported)");
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
	pxAssertDev(!(value & STATE_FLAG_WAITING_EMPTY), "Multiple threads attempted to wait for empty (not currently supported)");
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
