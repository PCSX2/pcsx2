/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021 PCSX2 Dev Team
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

#include "GS.h"
#include "common/boost_spsc_queue.hpp"
#include "common/General.h"
#include "common/Threading.h"
#include <condition_variable>
#include <functional>
#include <mutex>

template <class T, int CAPACITY>
class GSJobQueue final
{
private:
	std::thread m_thread;
	std::function<void()> m_startup;
	std::function<void(T&)> m_func;
	std::function<void()> m_shutdown;
	bool m_exit;
	ringbuffer_base<T, CAPACITY> m_queue;

	Threading::WorkSema m_sema;

	void ThreadProc()
	{
		if (m_startup)
			m_startup();

		while (true)
		{
			m_sema.WaitForWorkWithSpin();
			if (m_exit)
				break;
			while (m_queue.consume_one(*this))
				;
		}

		if (m_shutdown)
			m_shutdown();
	}

public:
	GSJobQueue(std::function<void()> startup, std::function<void(T&)> func, std::function<void()> shutdown)
		: m_startup(std::move(startup))
		, m_func(std::move(func))
		, m_shutdown(std::move(shutdown))
		, m_exit(false)
	{
		m_thread = std::thread(&GSJobQueue::ThreadProc, this);
	}

	~GSJobQueue()
	{
		m_exit = true;
		m_sema.NotifyOfWork();
		m_thread.join();
	}

	bool IsEmpty()
	{
		return m_queue.empty();
	}

	void Push(const T& item)
	{
		while (!m_queue.push(item))
			std::this_thread::yield();
		m_sema.NotifyOfWork();
	}

	void Wait()
	{
		m_sema.WaitForEmptyWithSpin();
		assert(IsEmpty());
	}

	void operator()(T& item)
	{
		m_func(item);
	}
};
