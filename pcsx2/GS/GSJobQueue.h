// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "GS.h"
#include "common/boost_spsc_queue.hpp"
#include "common/Assertions.h"
#include "common/Threading.h"
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

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
		pxAssert(IsEmpty());
	}

	void operator()(T& item)
	{
		m_func(item);
	}
};
