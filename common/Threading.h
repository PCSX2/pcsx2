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

#pragma once

#include "common/Pcsx2Defs.h"
#include "common/General.h"

#if defined(__APPLE__)
#include <mach/semaphore.h>
#elif !defined(_WIN32)
#include <semaphore.h>
#endif

#include <atomic>

namespace Threading
{
	// --------------------------------------------------------------------------------------
	//  Platform Specific External APIs
	// --------------------------------------------------------------------------------------
	// The following set of documented functions have Linux/Win32 specific implementations,
	// which are found in WinThreads.cpp and LnxThreads.cpp

	extern u64 GetThreadCpuTime();
	extern u64 GetThreadTicksPerSecond();

	/// Set the name of the current thread
	extern void SetNameOfCurrentThread(const char* name);

	// Releases a timeslice to other threads.
	extern void Timeslice();

	// For use in spin/wait loops.
	extern void SpinWait();

	// Optional implementation to enable hires thread/process scheduler for the operating system.
	// Needed by Windows, but might not be relevant to other platforms.
	extern void EnableHiresScheduler();
	extern void DisableHiresScheduler();

	// sleeps the current thread for the given number of milliseconds.
	extern void Sleep(int ms);

	// --------------------------------------------------------------------------------------
	//  ThreadHandle
	// --------------------------------------------------------------------------------------
	// Abstracts an OS's handle to a thread, closing the handle when necessary. Currently,
	// only used for getting the CPU time for a thread.
	//
	class ThreadHandle
	{
	public:
		ThreadHandle();
		ThreadHandle(ThreadHandle&& handle);
		ThreadHandle(const ThreadHandle& handle);
		~ThreadHandle();

		/// Returns a new handle for the calling thread.
		static ThreadHandle GetForCallingThread();

		ThreadHandle& operator=(ThreadHandle&& handle);
		ThreadHandle& operator=(const ThreadHandle& handle);

		operator void*() const { return m_native_handle; }
		operator bool() const { return (m_native_handle != nullptr); }

		/// Returns the amount of CPU time consumed by the thread, at the GetThreadTicksPerSecond() frequency.
		u64 GetCPUTime() const;

		/// Sets the affinity for a thread to the specified processors.
		/// Obviously, only works up to 64 processors.
		bool SetAffinity(u64 processor_mask) const;

	private:
		void* m_native_handle = nullptr;

		// We need the thread ID for affinity adjustments on Linux.
#if defined(__linux__)
		unsigned int m_native_id = 0;
#endif
	};

	/// A semaphore that may not have a fast userspace path
	/// (Used in other semaphore-based algorithms where the semaphore is just used for its thread sleep/wake ability)
	class KernelSemaphore
	{
#if defined(_WIN32)
		void* m_sema;
#elif defined(__APPLE__)
		semaphore_t m_sema;
#else
		sem_t m_sema;
#endif
	public:
		KernelSemaphore();
		~KernelSemaphore();
		void Post();
		void Wait();
		bool TryWait();
	};

	/// A semaphore for notifying a work-processing thread of new work in a (separate) queue
	///
	/// Usage:
	/// - Processing thread loops on `WaitForWork()` followed by processing all work in the queue
	/// - Threads adding work first add their work to the queue, then call `NotifyOfWork()`
	class WorkSema
	{
		/// Semaphore for sleeping the worker thread
		KernelSemaphore m_sema;
		/// Semaphore for sleeping thread waiting on worker queue empty
		KernelSemaphore m_empty_sema;
		/// Current state (see enum below)
		std::atomic<s32> m_state{0};

		// Expected call frequency is NotifyOfWork > WaitForWork > WaitForEmpty
		// So optimize states for fast NotifyOfWork
		enum
		{
			/* Any <-2 state: STATE_DEAD: Thread has crashed and is awaiting revival */
			STATE_SPINNING = -2, ///< Worker thread is spinning waiting for work
			STATE_SLEEPING = -1, ///< Worker thread is sleeping on m_sema
			STATE_RUNNING_0 = 0, ///< Worker thread is processing work, but no work has been added since it last checked for new work
			/* Any >0 state: STATE_RUNNING_N: Worker thread is processing work, and work has been added since it last checked for new work */
			STATE_FLAG_WAITING_EMPTY = 1 << 30, ///< Flag to indicate that a thread is sleeping on m_empty_sema (can be applied to any STATE_RUNNING)
		};

		bool IsDead(s32 state)
		{
			return state < STATE_SPINNING;
		}

		bool IsReadyForSleep(s32 state)
		{
			s32 waiting_empty_cleared = state & (STATE_FLAG_WAITING_EMPTY - 1);
			return waiting_empty_cleared == STATE_RUNNING_0;
		}

		s32 NextStateWaitForWork(s32 current)
		{
			s32 new_state = IsReadyForSleep(current) ? STATE_SLEEPING : STATE_RUNNING_0;
			return new_state | (current & STATE_FLAG_WAITING_EMPTY); // Preserve waiting empty flag for RUNNING_N → RUNNING_0
		}

	public:
		/// Notify the worker thread that you've added new work to its queue
		void NotifyOfWork()
		{
			// State change:
			// DEAD: Stay in DEAD (starting DEAD state is INT_MIN so we can assume we won't flip over to anything else)
			// SPINNING: Change state to RUNNING.  Thread will notice and process the new data
			// SLEEPING: Change state to RUNNING and wake worker.  Thread will wake up and process the new data.
			// RUNNING_0: Change state to RUNNING_N.
			// RUNNING_N: Stay in RUNNING_N
			s32 old = m_state.fetch_add(2, std::memory_order_release);
			if (old == STATE_SLEEPING)
				m_sema.Post();
		}

		/// Wait for work to be added to the queue
		void WaitForWork();
		/// Wait for work to be added to the queue, spinning for a bit before sleeping the thread
		void WaitForWorkWithSpin();
		/// Wait for the worker thread to finish processing all entries in the queue or die
		/// Returns false if the thread is dead
		bool WaitForEmpty();
		/// Wait for the worker thread to finish processing all entries in the queue or die, spinning a bit before sleeping the thread
		/// Returns false if the thread is dead
		bool WaitForEmptyWithSpin();
		/// Called by the worker thread to notify others of its death
		/// Dead threads don't process work, and WaitForEmpty will return instantly even though there may be work in the queue
		void Kill();
		/// Reset the semaphore to the initial state
		/// Should be called by the worker thread if it restarts after dying
		void Reset();
	};
} // namespace Threading
